#include "MessageCenter.h"

#include "NormalEvent.h"

using namespace Sloong;
using namespace Sloong::Universal;
using namespace Sloong::Events;


CMessageCenter::CMessageCenter()
{
}


CMessageCenter::~CMessageCenter()
{
}

void CMessageCenter::Initialize(int nWorkLoopNum, int ThreadPoolNum)
{
	CThreadPool::Initialize(ThreadPoolNum);
	CThreadPool::AddWorkThread(MessageWorkLoop, this, nWorkLoopNum);
}

void CMessageCenter::SendMessage(MSG_TYPE msgType)
{
	CNormalEvent* evt = new CNormalEvent();
	evt->SetEvent(msgType);
	unique_lock<mutex> lck(m_oMsgListMutex);
	m_oMsgList.push(evt);
	m_oWrokLoopCV.notify_one();
}

void CMessageCenter::SendMessage(IEvent * evt)
{
	unique_lock<mutex> lck(m_oMsgListMutex);
	m_oMsgList.push(evt);
	m_oWrokLoopCV.notify_one();
}

void CMessageCenter::CallMessage(MSG_TYPE msgType, void * msgParams)
{
}

void CMessageCenter::RegisterEvent(MSG_TYPE t)
{
	m_oMsgHandlerList[t] = vector<HandlerItem>();
}

void CMessageCenter::RegisterEventHandler(MSG_TYPE t, void* object, LPCALLBACK2FUNC func)
{
	if (m_oMsgHandlerList.find(t) == m_oMsgHandlerList.end())
	{
		throw normal_except("Target event is not regist.");
	}
	else
	{
		HandlerItem item;
		item.handler = func;
		item.object = object;
		m_oMsgHandlerList[t].push_back(item);
	}
}

void CMessageCenter::Run()
{
	m_emStatus = RUN_STATUS::Running;
}

void CMessageCenter::Exit()
{
	m_emStatus = RUN_STATUS::Exit;
}

void * CMessageCenter::MessageWorkLoop(void * param)
{
	CMessageCenter* pThis = (CMessageCenter*)param;
	unique_lock<mutex> work_lck(pThis->m_oWorkLoopMutex);
	while (pThis->m_emStatus != RUN_STATUS::Exit)
	{
		try
		{
			if (pThis->m_emStatus == RUN_STATUS::Created)
			{
				SLEEP(100);
				continue;
			}
			if (pThis->m_oMsgList.empty())
			{
				pThis->m_oWrokLoopCV.wait(work_lck);
				continue;
			}
			if (!pThis->m_oMsgList.empty())
			{
				unique_lock<mutex> lck(pThis->m_oMsgListMutex);
				if (pThis->m_oMsgList.empty())
				{
					lck.unlock();
					continue;
				}

				auto p = pThis->m_oMsgList.front();
				pThis->m_oMsgList.pop();
				lck.unlock();

				// Get the message handler list.
				auto evt_type = p->GetEvent();
				auto handler_list = pThis->m_oMsgHandlerList[evt_type];
				int handler_num = handler_list.size();
				if ( handler_num == 0 )
					continue;

				// �����漰���ദ�����������ͷű��������һ��ʹ�������ͷ�
				// ���������������뵽�������ǰ���Զ�����AddRef���������Ӽ�����
				// ���ڵ��Ĵ���������̫�죬�����´���δ���뵽�����У�ǰһ���Ѿ����н���ֱ�ӽ�Event����ɾ����
				// ����������һ���Խ����ü������ݶ��д�С������ϡ�
				p->AddRef(handler_num);
				for (int i = 0; i < handler_num; i++)
				{
					auto item = handler_list[i];
					// Use the threadpool to do the message 	
					CThreadPool::EnqueTask2(item.handler, p, item.object);
				}
			}
		}
		catch (...)
		{
			cerr << "Unhandle exception in MessageCenter work loop." << endl;
		}
		
	}

	return nullptr;
}
