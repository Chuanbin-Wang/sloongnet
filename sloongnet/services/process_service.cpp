/* File Name: server.c */
#include "process_service.h"
#include "NetworkHub.h"
#include "ControlHub.h"
#include "LuaProcessCenter.h"
#include "IData.h"
#include "utility.h"
#include "NetworkEvent.hpp"
#include "DataTransPackage.h"
using namespace Sloong;
using namespace Sloong::Events;


int main( int argc, char** args )
{
	try
	{
		Sloong::CSloongBaseService::g_pAppService = make_unique<SloongNetProcess>();

		auto res = Sloong::CSloongBaseService::g_pAppService->Initialize(argc, args);
		if (res.IsSucceed()){
			Sloong::CSloongBaseService::g_pAppService->Run();
			return 0;
		}
		else{
			cout << "Initialize error. message: " << res.Message() << endl;
			return -1;
		}
	}
	catch (...)
	{
		cout << "Unhandle exception happened, system will shutdown. " << endl;
		CUtility::write_call_stack();
	}
}



CResult SloongNetProcess::Initialize(int argc, char** args)
{
	auto res = CSloongBaseService::Initialize(argc,args);
	if( !res.IsSucceed())
		return res;
	if(!m_oConfig.ParseFromString(m_szConfigData))
		return CResult(false,"Parse the config struct error.");
	else
		cout << "Parse special configuation succeed." << endl;
	
	m_pControl->Add(DATA_ITEM::ModuleConfiguation, &m_oConfig);
	m_pNetwork->RegisterMessageProcesser(std::bind(&SloongNetProcess::MessagePackageProcesser, this, std::placeholders::_1));
	m_pControl->RegisterEventHandler(SocketClose, std::bind(&SloongNetProcess::OnSocketClose, this, std::placeholders::_1));
	m_pProcess->Initialize(m_pControl.get());
	
	return CResult::Succeed;
}

void Sloong::SloongNetProcess::MessagePackageProcesser(SmartPackage pack)
{	
	string strRes("");
	char* pExData = nullptr;
	int nExSize;

	auto msg = pack->GetRecvPackage();
	switch((MessageFunction)msg->function())
	{
		case MessageFunction::ProcessMessage:
			string uuid = msg->extenddata();
			auto infoItem = m_mapUserInfoList.find(uuid);
			if( infoItem == m_mapUserInfoList.end() )
			{
				m_mapUserInfoList[uuid] = make_unique<CLuaPacket>();
				infoItem= m_mapUserInfoList.find(uuid);
			}
			if (m_pProcess->MsgProcess(infoItem->second.get(), msg->context() , strRes, pExData, nExSize)){
				msg->set_context(strRes);
			}else{
				m_pLog->Error("Error in process");
				msg->set_context("{\"errno\": \"-1\",\"errmsg\" : \"server process happened error\"}");
			}
			pack->ResponsePackage(msg);
			auto response_event = make_shared<CNetworkEvent>(EVENT_TYPE::SendMessage);
			response_event->SetSocketID(pack->GetSocketID());
			response_event->SetDataPackage(pack);
			m_pControl->CallMessage(response_event);
		break;
	}

}

void Sloong::SloongNetProcess::OnSocketClose(SmartEvent event)
{
	auto net_evt = dynamic_pointer_cast<CNetworkEvent>(event);
	auto info = net_evt->GetUserInfo();
	if (!info)
	{
		m_pLog->Error(CUniversal::Format("Get socket info from socket list error, the info is NULL. socket id is: %d", net_evt->GetSocketID()));
		return;
	}
	// call close function.
	//m_pProcess->CloseSocket(info);
	//net_evt->CallCallbackFunc(net_evt);
}