/* File Name: server.c */
#include "gateway_service.h"
#include "utility.h"
#include "IData.h"
#include "SendMessageEvent.hpp"
using namespace Sloong;
using namespace Sloong::Events;

#include "protocol/manager.pb.h"
using namespace Manager;

#include "protocol/processer.pb.h"


unique_ptr<SloongNetGateway> Sloong::SloongNetGateway::Instance = nullptr;
mutex processmutex;

extern "C" CResult RequestPackageProcesser(void *env, CDataTransPackage *pack)
{
	unique_lock<mutex> lock(processmutex);
	return SloongNetGateway::Instance->RequestPackageProcesser(env, pack);
}

extern "C" CResult ResponsePackageProcesser(void *env, CDataTransPackage *pack)
{
	unique_lock<mutex> lock(processmutex);
	return SloongNetGateway::Instance->ResponsePackageProcesser(pack);
}

extern "C" CResult EventPackageProcesser(CDataTransPackage *pack)
{
	SloongNetGateway::Instance->EventPackageProcesser(pack);
	return CResult::Succeed();
} 

extern "C" CResult NewConnectAcceptProcesser(CSockInfo *info)
{
	return CResult::Succeed();
}

extern "C" CResult ModuleInitialization(GLOBAL_CONFIG *confiog)
{
	SloongNetGateway::Instance = make_unique<SloongNetGateway>();
	return CResult::Succeed();
}

extern "C" CResult ModuleInitialized(SOCKET sock, IControl *iC)
{
	return SloongNetGateway::Instance->Initialized(sock, iC);
}

extern "C" CResult CreateProcessEnvironment(void **out_env)
{
	return SloongNetGateway::Instance->CreateProcessEnvironmentHandler(out_env);
}

CResult SloongNetGateway::Initialized(SOCKET sock, IControl *iC)
{
	m_pControl = iC;
	IData::Initialize(iC);
	m_pConfig = IData::GetGlobalConfig();
	m_pModuleConfig = IData::GetModuleConfig();
	m_pRuntimeData = IData::GetRuntimeData();
	if (m_pModuleConfig)
	{
		shared_ptr<CNormalEvent> event = make_shared<CNormalEvent>();
		event->SetEvent(EVENT_TYPE::EnableTimeoutCheck);
		event->SetMessage(Helper::Format("{\"TimeoutTime\":\"%d\", \"CheckInterval\":%d}", (*m_pModuleConfig)["TimeoutTime"].asInt(), (*m_pModuleConfig)["TimeoutCheckInterval"].asInt()));
		m_pControl->SendMessage(event);

		event->SetEvent(EVENT_TYPE::EnableClientCheck);
		event->SetMessage(Helper::Format("{\"ClientCheckKey\":\"%s\", \"ClientCheckTime\":%d}", (*m_pModuleConfig)["ClientCheckKey"].asString().c_str(), (*m_pModuleConfig)["ClientCheckKey"].asInt()));
		m_pControl->SendMessage(event);
	}
	m_pLog = IData::GetLog();
	m_nManagerConnection = sock;
	m_pControl->RegisterEventHandler(EVENT_TYPE::ProgramStart, std::bind(&SloongNetGateway::OnStart, this, std::placeholders::_1));
	m_pControl->RegisterEventHandler(EVENT_TYPE::SocketClose, std::bind(&SloongNetGateway::OnSocketClose, this, std::placeholders::_1));
	m_pControl->RegisterEventHandler(EVENT_TYPE::SendPackage, std::bind(&SloongNetGateway::SendPackageHook, this, std::placeholders::_1));
	return CResult::Succeed();
}

CResult SloongNetGateway::RequestPackageProcesser(void *env, CDataTransPackage *trans_pack)
{
	m_pLog->Debug("Receive new request package.");
	auto res = MessageToProcesser(trans_pack);
	m_pLog->Debug(Helper::Format("Response [%s][%s].", Core::ResultType_Name(res.Result()).c_str(), res.Message().c_str()));
	//trans_pack->ResponsePackage(res);
	return res;
}

CResult SloongNetGateway::ResponsePackageProcesser(CDataTransPackage *trans_pack)
{
	auto num = trans_pack->GetSerialNumber();
	if( m_mapSerialToRequest.exist(num) )
	{
		return MessageToClient(&m_mapSerialToRequest[num],trans_pack);
	}

	if (!m_listSendEvent.exist(num))
		m_pLog->Error("ResponsePackageProcesser no find the package");

	auto send_evt = dynamic_pointer_cast<CSendPackageEvent>(m_listSendEvent[num]);
	auto need_send_res = send_evt->CallCallbackFunc(trans_pack);
	m_listSendEvent.erase(num);
	return need_send_res;
}

void SloongNetGateway::QueryReferenceInfo()
{
	auto event = make_shared<CSendPackageEvent>();
	event->SetCallbackFunc(std::bind(&SloongNetGateway::QueryReferenceInfoResponseHandler, this, std::placeholders::_1, std::placeholders::_2));
	event->SetRequest(m_nManagerConnection, m_pRuntimeData->nodeuuid(), m_nSerialNumber, 1, (int)Functions::QueryReferenceInfo, "");
	m_nSerialNumber++;
	m_pControl->SendMessage(event);
}

inline int SloongNetGateway::ParseFunctionValue(const string &s)
{
	int res = 0;
	auto nFunc = ConvertStrToInt(s, -1, &res);
	if (nFunc == -1)
		m_pLog->Error(Helper::Format("Parse function string[%s] to int error[%d].", s.c_str(), res));
	return nFunc;
}

// process the provied function string to list.
list<int> SloongNetGateway::ProcessProviedFunction(const string &prov_func)
{
	list<int> res_list;
	auto funcs = Helper::split(prov_func, ',');
	for (auto func : funcs)
	{
		if (func.find("-") != string::npos)
		{
			auto range = Helper::split(func, '-');
			auto start = ParseFunctionValue(range[0]);
			auto end = ParseFunctionValue(range[1]);
			if (start == -1 || end == -1)
				return res_list;
			for (int i = start; i <= end; i++)
			{
				res_list.push_back(i);
			}
		}
		else
		{
			auto nFunc = ParseFunctionValue(func);
			if (nFunc == -1)
				return res_list;
			res_list.push_back(nFunc);
		}
	}
	return res_list;
}

CResult SloongNetGateway::QueryReferenceInfoResponseHandler(IEvent *send_pack, CDataTransPackage *res_pack)
{
	auto str_res = res_pack->GetRecvMessage();
	auto res = ConvertStrToObj<QueryReferenceInfoResponse>(str_res);
	if (res == nullptr || res->templateinfos_size() == 0)
		return CResult::Invalid();

	auto templateInfos = res->templateinfos();
	for (auto info : templateInfos)
	{
		if (info.providefunctions() == "*")
			m_mapFuncToTemplateIDs[-1].unique_insert(info.templateid());
		else
		{
			for (auto i : ProcessProviedFunction(info.providefunctions()))
				m_mapFuncToTemplateIDs[i].unique_insert(info.templateid());
		}
		for (auto item : info.nodeinfos())
		{
			m_mapUUIDToNode[item.uuid()] = item;
			m_mapTempteIDToUUIDs[info.templateid()].push_back(item.uuid());

			AddConnection(item.uuid(), item.address(), item.port());
		}
	}
	return CResult::Invalid();
}

void SloongNetGateway::AddConnection(const string &uuid, const string &addr, int port)
{
	EasyConnect conn;
	conn.Initialize(addr, port);
	conn.Connect();
	auto event = make_shared<CNetworkEvent>(EVENT_TYPE::RegisteConnection);
	event->SetSocketID(conn.GetSocketID());
	m_pControl->SendMessage(event);
	m_mapUUIDToConnect[uuid] = conn.GetSocketID();
}

CResult SloongNetGateway::CreateProcessEnvironmentHandler(void **out_env)
{
	/*auto item = make_shared<GatewayTranspond>();
	auto res = item->Initialize(m_pControl);
	if (res.IsFialed())
		return res;
	m_listTranspond.push_back(item);
	(*out_env) = item.get();*/
	return CResult::Succeed();
}

void SloongNetGateway::SendPackageHook(SmartEvent event)
{
	auto send_evt = dynamic_pointer_cast<CSendPackageEvent>(event);
	auto pack = send_evt->GetDataPackage();
	m_listSendEvent[pack->serialnumber()] = event;
}

void SloongNetGateway::OnStart(SmartEvent evt)
{
	QueryReferenceInfo();
}

void Sloong::SloongNetGateway::OnSocketClose(SmartEvent event)
{
	auto net_evt = dynamic_pointer_cast<CNetworkEvent>(event);
	auto info = net_evt->GetUserInfo();
	if (!info)
	{
		m_pLog->Error(Helper::Format("Get socket info from socket list error, the info is NULL. socket id is: %d", net_evt->GetSocketID()));
		return;
	}
}

void Sloong::SloongNetGateway::OnReferenceModuleOnlineEvent(const string &str_req, CDataTransPackage *trans_pack)
{
	m_pLog->Info("Receive ReferenceModuleOnline event");
	auto req = ConvertStrToObj<Manager::EventReferenceModuleOnline>(str_req);
	auto item = req->item();
	m_mapUUIDToNode[item.uuid()] = item;
	m_mapTempteIDToUUIDs[item.templateid()].push_back(item.uuid());
	m_pLog->Debug(Helper::Format("New module is online:[%s][%s:%d]", item.uuid().c_str(), item.address().c_str(), item.port()));

	AddConnection(item.uuid(), item.address(), item.port());
}

void Sloong::SloongNetGateway::OnReferenceModuleOfflineEvent(const string &str_req, CDataTransPackage *trans_pack)
{
	m_pLog->Info("Receive ReferenceModuleOffline event");
	auto req = ConvertStrToObj<Manager::EventReferenceModuleOffline>(str_req);
	auto uuid = req->uuid();
	auto item = m_mapUUIDToNode[uuid];
	m_mapTempteIDToUUIDs[item.templateid()].erase(item.uuid());
	auto event = make_shared<CNetworkEvent>(EVENT_TYPE::SocketClose);
	event->SetSocketID(m_mapUUIDToConnect[uuid]);
	m_pControl->SendMessage(event);
	m_mapUUIDToConnect.erase(uuid);
	m_mapUUIDToNode.erase(uuid);
}

void Sloong::SloongNetGateway::EventPackageProcesser(CDataTransPackage *trans_pack)
{
	auto data_pack = trans_pack->GetDataPackage();
	auto event = (Manager::Events)data_pack->function();
	if (!Manager::Events_IsValid(event))
	{
		m_pLog->Error(Helper::Format("EventPackageProcesser is called.but the fucntion[%d] check error.", event));
		return;
	}

	switch (event)
	{
	case Manager::Events::ReferenceModuleOnline:
	{
		OnReferenceModuleOnlineEvent(data_pack->content(), trans_pack);
	}
	break;
	case Manager::Events::ReferenceModuleOffline:
	{
		OnReferenceModuleOfflineEvent(data_pack->content(), trans_pack);
	}
	break;
	default:
	{
		m_pLog->Error(Helper::Format("Event is no processed. [%s][%d].", Manager::Events_Name(event).c_str(), event));
	}
	break;
	}
}


SOCKET Sloong::SloongNetGateway::GetPorcessConnect(int function)
{
	if (!m_mapFuncToTemplateIDs.exist(function) && !m_mapFuncToTemplateIDs.exist(-1))
	{
		return INVALID_SOCKET;
	}

	for( auto tpl : m_mapFuncToTemplateIDs[function])
	{
		if (m_mapTempteIDToUUIDs[tpl].size() == 0)
			continue;

		for (auto node : m_mapTempteIDToUUIDs[tpl])
		{
			return m_mapUUIDToConnect[node];
		}
	}

	for( auto tpl : m_mapFuncToTemplateIDs[-1])
	{
		if (m_mapTempteIDToUUIDs[tpl].size() == 0)
			continue;

		for (auto node : m_mapTempteIDToUUIDs[tpl])
		{
			return m_mapUUIDToConnect[node];
		}
	}

	return INVALID_SOCKET;
}


CResult Sloong::SloongNetGateway::MessageToProcesser(CDataTransPackage *pack)
{
	auto data_pack = pack->GetDataPackage();
	auto target = GetPorcessConnect(data_pack->function());
	if( target == INVALID_SOCKET )
	{
		return CResult::Make_Error("No process service online .");
	}

	RequestInfo info;
	info.RequestConnect = pack->GetConnection();
	info.SerialNumber = data_pack->serialnumber();	
		
	auto serialNumber = GetSerialNumber();
	data_pack->set_serialnumber(serialNumber);
	pack->ClearConnection();
	pack->SetSocket(target);	
	pack->RequestPackage();

	m_pLog->Debug(Helper::Format("Trans package [%d][%d] -> [%d][%d]", info.RequestConnect->GetSocketID(), info.SerialNumber, target, serialNumber));

	m_mapSerialToRequest[serialNumber] = info;
	return CResult::Succeed();
}

CResult Sloong::SloongNetGateway::MessageToClient(RequestInfo *req_info, CDataTransPackage *res_pack)
{
	auto res_data = res_pack->GetDataPackage();
	res_data->set_serialnumber(req_info->SerialNumber);
	res_pack->SetConnection(req_info->RequestConnect);
	res_pack->ResponsePackage(ResultType::Succeed);

	return CResult::Succeed();
}