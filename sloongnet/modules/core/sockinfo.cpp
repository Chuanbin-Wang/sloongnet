#include "sockinfo.h"
#include <univ/luapacket.h>
#include "DataTransPackage.h"
#include "NetworkEvent.hpp"
#include "IData.h"
using namespace Sloong;
using namespace Sloong::Universal;
using namespace Sloong::Events;
Sloong::CSockInfo::CSockInfo()
{
	m_pSendList = new queue<SmartPackage>[s_PriorityLevel]();
	m_pCon = make_shared<EasyConnect>();
}

CSockInfo::~CSockInfo()
{
	for (int i = 0; i < s_PriorityLevel;i++){
		while (!m_pSendList[i].empty()){
			m_pSendList[i].pop();
        }
	}
	SAFE_DELETE_ARR(m_pSendList);

    while (!m_oPrepareSendList.empty()){
        m_oPrepareSendList.pop();
    }
}


void Sloong::CSockInfo::Initialize(IControl* iMsg, int sock, SSL_CTX* ctx)
{
	IObject::Initialize(iMsg);
	m_ActiveTime = time(NULL);
	auto serv_config = IData::GetGlobalConfig();
	m_ReceiveTimeout = serv_config->receivetime();
	m_pCon->Initialize(sock,ctx);
}

ResultType Sloong::CSockInfo::ResponseDataPackage(SmartPackage pack)
{	
	// if have exdata, directly add to epoll list.
	if (pack->IsBigPackage()){
		AddToSendList(pack);
		return ResultType::Retry;
	}else{
		// check the send list size. if all empty, try send message directly.
		if ((m_bIsSendListEmpty == false && !m_oPrepareSendList.empty()) || m_oSockSendMutex.try_lock() == false){
			AddToSendList(pack);
			return ResultType::Retry;
		}
	}

	unique_lock<mutex> lck(m_oSockSendMutex, std::adopt_lock);
	// if code run here. the all list is empty. and no have exdata. try send message
	auto res = pack->SendPackage();
	if ( res == ResultType::Error ){
		// TODO: 这里应该对错误进行区分处理
		m_pLog->Warn(CUniversal::Format("Send data failed.[%s]", m_pCon->m_strAddress));//, m_pCon->G_FormatSSLErrorMsg(nMsgSend)));
		return ResultType::Error;
	}
	if (res == ResultType::Retry ){
		AddToSendList(pack);
		return ResultType::Retry;
	}
	lck.unlock();
	return ResultType::Succeed;
}


void Sloong::CSockInfo::AddToSendList(SmartPackage pack)
{
	unique_lock<mutex> lck(m_oPreSendMutex);
	m_oPrepareSendList.push(pack);
	m_pLog->Debug(CUniversal::Format("Add send package to prepare send list. list size:[%d]",m_oPrepareSendList.size()));
	m_bIsSendListEmpty = false;
}


ResultType Sloong::CSockInfo::OnDataCanReceive( queue<SmartPackage>& readList )
{
	unique_lock<mutex> srlck(m_oSockReadMutex);

	// 已经连接的用户,收到数据,可以开始读入
	bool bLoop = false;
	do {
		auto package = make_shared<CDataTransPackage>(m_pCon);
		auto res = package->RecvPackage(m_ReceiveTimeout);
		if( !bLoop && res == ResultType::Error){
			// 读取错误,将这个连接从监听中移除并关闭连接
			return ResultType::Error;
		}else if( !bLoop && res == ResultType::Invalid){
			auto event = make_shared<CNetworkEvent>(EVENT_TYPE::MonitorSendStatus);
			event->SetSocketID(m_pCon->GetSocketID());
			m_iC->SendMessage(event);
			AddToSendList(package);
		}else if ( res == ResultType::Succeed ){
			bLoop = true;
			// update the socket time
			m_ActiveTime = time(NULL);
			readList.push(package);
		}else{
			//由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可读在这里就当作是该次事件已处理过。
			// 或者是在第二次读取的时候，发生了错误，仍视为成功
			return ResultType::Succeed;
		}
	}while (bLoop);

	srlck.unlock();
	return ResultType::Succeed;
}


ResultType Sloong::CSockInfo::OnDataCanSend()
{
	ProcessPrepareSendList();
	return ProcessSendList();
}



void Sloong::CSockInfo::ProcessPrepareSendList()
{
	// progress the prepare send list first
	if (!m_oPrepareSendList.empty()){
		unique_lock<mutex> prelck(m_oPreSendMutex);
		if (m_oPrepareSendList.empty())
			return;
		
		unique_lock<mutex> sendListlck(m_oSendListMutex);
		
		while (!m_oPrepareSendList.empty()){
			auto pack = m_oPrepareSendList.front();
			m_oPrepareSendList.pop();
			auto priority = pack->GetPriority();
			m_pSendList[priority].push(pack);
			m_pLog->Debug(CUniversal::Format("Add send package to send list[%d]. send list size[%d], prepare send list size[%d]",
								priority,m_pSendList[priority].size(),m_oPrepareSendList.size()));
		}
		prelck.unlock();
		sendListlck.unlock();
	}
}



ResultType Sloong::CSockInfo::ProcessSendList()
{
	// when prepare list process done, do send operation.
	bool bTrySend = true;

	// 这里始终从list开始循环，保证高优先级的信息先被处理
	while (bTrySend)
	{
		unique_lock<mutex> lck(m_oSendListMutex);

		queue<shared_ptr<CDataTransPackage>>* list = nullptr;
		int sendTags = GetSendInfoList(list);
		if (list == nullptr){
			m_pLog->Error("Send info list empty, no need send.");
			break;
		}

		// if no find send info, is no need send anything , remove this sock from epoll.'
		auto si = GetSendInfo(list);
		if ( si != nullptr )
		{
			lck.unlock();
			unique_lock<mutex> ssend_lck(m_oSockSendMutex);
			int res =si->SendPackage();
			ssend_lck.unlock();
			if( res < 0){
				m_pLog->Error(CUniversal::Format("Send data package error. close connect:[%s:%d]",m_pCon->m_strAddress,m_pCon->m_nPort));
				return ResultType::Error;
			}else if( res == 0){
				m_pLog->Verbos("Send data package done. wait next write sign.");
				bTrySend = false;
				m_nLastSentTags = sendTags;
				return ResultType::Retry;
			}else{
				list->pop();
				m_nLastSentTags = -1;
				bTrySend = true;
			}
		}
	}
	return ResultType::Succeed;
}


/// 获取发送信息列表
// 首先判断上次发送标志，如果不为-1，表示上次的发送列表没有发送完成。直接返回指定的列表
// 如果为-1，表示需要发送新的列表。按照优先级逐级的进行寻找。
int Sloong::CSockInfo::GetSendInfoList( queue<shared_ptr<CDataTransPackage>>*& list )
{
	list = nullptr;
	// prev package no send end. find and try send it again.
	if (-1 != m_nLastSentTags)
	{
		m_pLog->Verbos(CUniversal::Format("Send prev time list, Priority level:%d", m_nLastSentTags));
		list = &m_pSendList[m_nLastSentTags];
		if( list->empty() )
			m_nLastSentTags = -1;
		else
			return m_nLastSentTags;
	}
	
	for (int i = 0; i < s_PriorityLevel; i++)
	{
		if (m_pSendList[i].empty())
			continue;
		else
		{
			list = &m_pSendList[i];
			m_pLog->Verbos(CUniversal::Format("Send list, Priority level:%d", i));
			return i;
		}
	}
	return -1;
}


shared_ptr<CDataTransPackage> Sloong::CSockInfo::GetSendInfo(queue<shared_ptr<CDataTransPackage>>* list)
{
	shared_ptr<CDataTransPackage> si = nullptr;
	while (si == nullptr)
	{
		if (!list->empty())
		{
			m_pLog->Verbos(CUniversal::Format("Get send info from list, list size[%d].", list->size()));
			si = list->front();
			if (si == nullptr)
			{
				m_pLog->Verbos("The list front is NULL, pop it and get next.");
				list->pop();
			}
		}
		else
		{
			// the send list is empty, so no need loop.
			m_pLog->Verbos("Send list is empty list. no need send message");
			break;
		}
	}
	if (si == nullptr)
	{
		if (m_nLastSentTags != -1)
		{
			m_pLog->Verbos("Current list no send message, clear the LastSentTags flag.");
			m_nLastSentTags = -1;
		}
		else
		{
			m_pLog->Verbos(CUniversal::Format("No message need send, remove socket[%d] from Epoll", m_pCon->GetSocketID()));
			m_bIsSendListEmpty = true;
		}
	}
	return si;
}