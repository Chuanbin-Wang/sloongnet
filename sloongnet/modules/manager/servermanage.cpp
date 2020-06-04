/*
 * @Author: WCB
 * @Date: 2020-04-29 09:27:21
 * @LastEditors: WCB
 * @LastEditTime: 2020-05-18 19:19:53
 * @Description: file content
 */
#include "servermanage.h"

#include "utility.h"
#include "SendPackageEvent.hpp"

#include "snowflake.h"

using namespace Sloong::Events;

CResult Sloong::CServerManage::Initialize(IControl *ic)
{
	IObject::Initialize(ic);

	m_mapFuncToHandler[Functions::PostLog] = std::bind(&CServerManage::EventRecorderHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::RegisteWorker] = std::bind(&CServerManage::RegisteWorkerHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::RegisteNode] = std::bind(&CServerManage::RegisteNodeHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::AddTemplate] = std::bind(&CServerManage::AddTemplateHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::DeleteTemplate] = std::bind(&CServerManage::DeleteTemplateHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::SetTemplate] = std::bind(&CServerManage::SetTemplateHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::QueryTemplate] = std::bind(&CServerManage::QueryTemplateHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::QueryNode] = std::bind(&CServerManage::QueryNodeHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::QueryReferenceInfo] = std::bind(&CServerManage::QueryReferenceInfoHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::StopNode] = std::bind(&CServerManage::StopNodeHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::RestartNode] = std::bind(&CServerManage::RestartNodeHandler, this, std::placeholders::_1, std::placeholders::_2);
	m_mapFuncToHandler[Functions::ReportLoadStatus] = std::bind(&CServerManage::ReportLoadStatusHandler, this, std::placeholders::_1, std::placeholders::_2);

	if (!CConfiguation::Instance->IsInituialized())
	{
		auto res = CConfiguation::Instance->Initialize("/data/configuation.db");
		if (res.IsFialed())
			return res;
	}

	// Initialize template list
	auto list = CConfiguation::Instance->GetTemplateList();
	for (auto &item : list)
	{
		TemplateItem addItem(item);
		m_mapIDToTemplateItem[addItem.ID] = addItem;
		RefreshModuleReference(addItem.ID);
	}

	auto res = CConfiguation::Instance->GetTemplate(1);
	if (res.IsFialed())
		return CResult::Succeed();
	else
		return res;
}

CResult Sloong::CServerManage::ResetManagerTemplate(GLOBAL_CONFIG *config)
{
	string config_str;
	if (!config->SerializeToString(&config_str))
	{
		return CResult::Make_Error("Config SerializeToString error.");
	}
	TemplateItem item;
	item.ID = 1;
	item.Name = "Manager";
	item.Note = "This template just for the manager node.";
	item.Replicas = 1;
	item.Configuation = config_str;
	item.BuildCache();
	CResult res(ResultType::Succeed);
	auto info = item.ToTemplateInfo();
	if (CConfiguation::Instance->CheckTemplateExist(1))
		res = CConfiguation::Instance->SetTemplate(1, info);
	else
		res = CConfiguation::Instance->AddTemplate(info, nullptr);
	m_mapIDToTemplateItem[1] = item;
	return res;
}

int Sloong::CServerManage::SearchNeedCreateTemplate()
{
	// First time find the no created
	for (auto item : m_mapIDToTemplateItem)
	{
		if (item.second.Replicas == 0 || item.second.ID == 1)
			continue;

		if ((int)item.second.Created.size() >= item.second.Replicas)
			continue;

		if (item.second.Created.size() == 0)
			return item.first;
	}

	// Sencond time find the created < replicas
	for (auto item : m_mapIDToTemplateItem)
	{
		if (item.second.Replicas == 0 || item.second.ID == 1)
			continue;

		if ((int)item.second.Created.size() >= item.second.Replicas)
			continue;

		if ((int)item.second.Created.size() < item.second.Replicas)
			return item.first;
	}
	return -1;
}


void Sloong::CServerManage::SendEvent(const list<uint64_t> &notifyList, int event, ::google::protobuf::Message *msg)
{
	for (auto item : notifyList)
	{
		string msg_str;
		if (msg)
			msg->SerializeToString(&msg_str);
		auto req = make_unique<CSendPackageEvent>();
		req->SetRequest(m_mapUUIDToNodeItem[item].ConnectionID, IData::GetRuntimeData()->nodeuuid(), snowflake::Instance->nextid(), Core::HEIGHT_LEVEL, event, msg_str, "", DataPackage_PackageType::DataPackage_PackageType_EventPackage);
		m_iC->SendMessage(std::move(req));
	}
}

void Sloong::CServerManage::OnSocketClosed(SOCKET sock)
{
	if (!m_mapSocketToUUID.exist(sock))
		return;

	auto target = m_mapSocketToUUID[sock];
	auto id = m_mapUUIDToNodeItem[target].TemplateID;

	// Find reference node and notify them
	list<uint64_t> notifyList;
	for (auto &item : m_mapIDToTemplateItem)
	{
		if (item.second.Reference.exist(id))
		{
			for (auto i : item.second.Created)
				notifyList.push_back(i);
		}
	}

	if (notifyList.size() > 0)
	{
		EventReferenceModuleOffline offline_event;
		offline_event.set_uuid(target);
		SendEvent(notifyList, Manager::Events::ReferenceModuleOffline, &offline_event);
	}
	m_mapUUIDToNodeItem.erase(target);
	m_mapIDToTemplateItem[id].Created.remove(target);
}

CResult Sloong::CServerManage::ProcessHandler(CDataTransPackage *pack)
{
	auto function = (Functions)pack->GetFunction();
	if (!Manager::Functions_IsValid(function))
	{
		pack->ResponsePackage(ResultType::Error, Helper::Format("Parser request package function[%s] error.", pack->GetRecvMessage().c_str()));
		return CResult::Succeed();
	}

	auto req_str = pack->GetRecvMessage();
	auto func_name = Functions_Name(function);
	m_pLog->Debug(Helper::Format("Request [%d][%s]:[%s]", function, func_name.c_str(), req_str.c_str()));
	if (!m_mapFuncToHandler.exist(function))
	{
		pack->ResponsePackage(ResultType::Error, Helper::Format("Function [%s] no handler.", func_name.c_str()));
		return CResult::Succeed();
	}

	auto res = m_mapFuncToHandler[function](req_str, pack);
	m_pLog->Debug(Helper::Format("Response [%s]:[%s][%s].", func_name.c_str(), ResultType_Name(res.Result()).c_str(), res.Message().c_str()));
	if( res.Result() == ResultType::Ignore )
		return res;
	pack->ResponsePackage(res);
	return CResult::Succeed();
}

CResult Sloong::CServerManage::EventRecorderHandler(const string &req_str, CDataTransPackage *pack)
{
	return CResult::Succeed();
}

CResult Sloong::CServerManage::RegisteWorkerHandler(const string &req_str, CDataTransPackage *pack)
{
	auto sender = pack->GetSender();
	if (sender == 0)
	{
		sender = snowflake::Instance->nextid();
	}
	auto sender_info = m_mapUUIDToNodeItem.try_get(sender);
	if (sender_info == nullptr)
	{
		NodeItem item;
		item.Address = pack->GetSocketIP();
		item.UUID = sender;
		m_mapUUIDToNodeItem[sender] = item;
		m_pLog->Debug(Helper::Format("Module[%s:%d] regist to system. Allocating uuid [%llu].", item.Address.c_str(), item.Port, item.UUID));
		char m_pMsgBuffer[8] = {0};
		char *pCpyPoint = m_pMsgBuffer;
		Helper::Int64ToBytes(sender, pCpyPoint);
		return CResult(ResultType::Retry, string(m_pMsgBuffer, 8));
	}

	auto index = SearchNeedCreateTemplate();
	if (index == -1)
	{
		char m_pMsgBuffer[8] = {0};
		char *pCpyPoint = m_pMsgBuffer;
		Helper::Int64ToBytes(sender, pCpyPoint);
		return CResult(ResultType::Retry, string(m_pMsgBuffer, 8));
	}

	if (sender_info == nullptr)
	{
		return CResult::Make_Error("Add server info to ServerList fialed.");
	}

	auto tpl = m_mapIDToTemplateItem[index];
	RegisteWorkerResponse res;
	res.set_templateid(tpl.ID);
	res.set_configuation(m_mapIDToTemplateItem[tpl.ID].Configuation);

	m_pLog->Debug(Helper::Format("Allocating module[%llu] Type to [%s]", sender_info->UUID, tpl.Name.c_str()));
	return CResult::Make_OK(ConvertObjToStr(&res));
}

void Sloong::CServerManage::RefreshModuleReference(int id)
{
	auto info = m_mapIDToTemplateItem.try_get(id);
	if (info == nullptr)
		return;
	info->Reference.clear();
	auto references = Helper::split(info->ConfiguationObj->modulereference(), ';');
	for (auto &item : references)
	{
		int id;
		if (ConvertStrToInt(item, &id))
			info->Reference.push_back(id);
	}
}

CResult Sloong::CServerManage::RegisteNodeHandler(const string &req_str, CDataTransPackage *pack)
{
	auto sender = pack->GetSender();
	auto req = ConvertStrToObj<RegisteNodeRequest>(req_str);
	if (!req || sender == 0)
		return CResult::Make_Error("The required parameter check error.");

	int id = req->templateid();
	if (!m_mapIDToTemplateItem.exist(id))
		return CResult::Make_Error(Helper::Format("The template id [%d] is no exist.", id));

	if (!m_mapUUIDToNodeItem.exist(sender))
		return CResult::Make_Error(Helper::Format("The sender [%llu] is no regitser.", sender));

	if (id == 1)
		return CResult::Make_Error("Template id error.");

	// Save node info.
	auto &item = m_mapUUIDToNodeItem[sender];
	auto &tpl = m_mapIDToTemplateItem[id];
	item.TemplateName = tpl.Name;
	item.TemplateID = tpl.ID;
	item.Port = tpl.ConfiguationObj->listenport();
	item.ConnectionID = pack->GetSocketID();
	tpl.Created.unique_insert(sender);
	m_mapSocketToUUID[pack->GetSocketID()] = sender;

	// Find reference node and notify them
	list<uint64_t> notifyList;
	for (auto &item : m_mapIDToTemplateItem)
	{
		if (item.second.Reference.exist(id))
		{
			for (auto i : item.second.Created)
				notifyList.push_back(i);
		}
	}

	if (notifyList.size() > 0)
	{
		EventReferenceModuleOnline online_event;
		m_mapUUIDToNodeItem[sender].ToProtobuf(online_event.mutable_item());
		SendEvent(notifyList, Manager::Events::ReferenceModuleOnline, &online_event);
	}

	return CResult::Succeed();
}

CResult Sloong::CServerManage::AddTemplateHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<AddTemplateRequest>(req_str);
	auto info = req->addinfo();
	TemplateItem item;
	item.ID = 0;
	item.Name = info.name();
	item.Note = info.note();
	;
	item.Replicas = info.replicas();
	item.Configuation = info.configuation();
	item.BuildCache();
	if (!item.IsValid())
		return CResult::Make_Error("Param is valid.");

	int id = 0;
	auto res = CConfiguation::Instance->AddTemplate(item.ToTemplateInfo(), &id);
	if (res.IsFialed())
	{
		return res;
	}
	item.ID = id;
	m_mapIDToTemplateItem[id] = item;
	RefreshModuleReference(id);
	return CResult::Succeed();
}

CResult Sloong::CServerManage::DeleteTemplateHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<DeleteTemplateRequest>(req_str);

	int id = req->templateid();
	if (!m_mapIDToTemplateItem.exist(id))
	{
		return CResult::Make_Error(Helper::Format("The template id [%d] is no exist.", id));
	}
	if (id == 1)
	{
		return CResult::Make_Error("Cannot delete this template.");
	}

	auto res = CConfiguation::Instance->DeleteTemplate(id);
	if (res.IsFialed())
	{
		return res;
	}
	m_mapIDToTemplateItem.erase(id);
	return CResult::Succeed();
}

CResult Sloong::CServerManage::SetTemplateHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<SetTemplateRequest>(req_str);
	auto info = req->setinfo();
	if (!m_mapIDToTemplateItem.exist(info.id()))
	{
		return CResult::Make_Error("Check the templeate ID error, please check.");
	}

	auto tplInfo = m_mapIDToTemplateItem[info.id()];
	if (info.name().size() > 0)
		tplInfo.Name = info.name();

	if (info.note().size() > 0)
		tplInfo.Note = info.note();

	if (info.replicas() > 0)
		tplInfo.Replicas = info.replicas();

	if (info.configuation().size() > 0)
		tplInfo.Configuation = info.configuation();

	tplInfo.BuildCache();
	auto res = CConfiguation::Instance->SetTemplate(tplInfo.ID, tplInfo.ToTemplateInfo());
	if (res.IsFialed())
	{
		return res;
	}

	m_mapIDToTemplateItem[tplInfo.ID] = tplInfo;
	RefreshModuleReference(tplInfo.ID);
	return CResult::Succeed();
}

CResult Sloong::CServerManage::QueryTemplateHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<QueryTemplateRequest>(req_str);

	QueryTemplateResponse res;
	if (req->templateid_size() == 0)
	{
		for (auto &i : m_mapIDToTemplateItem)
		{
			i.second.ToProtobuf(res.add_templateinfos());
		}
	}
	else
	{
		auto ids = req->templateid();
		for (auto id : ids)
		{
			if (!m_mapIDToTemplateItem.exist(id))
			{
				return CResult::Make_Error(Helper::Format("The template id [%d] is no exist.", id));
			}
			m_mapIDToTemplateItem[id].ToProtobuf(res.add_templateinfos());
		}
	}

	return CResult::Make_OK(ConvertObjToStr(&res));
}

CResult Sloong::CServerManage::QueryNodeHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<QueryNodeRequest>(req_str);
	if (!req)
		return CResult::Make_Error("Parser message object fialed.");

	QueryNodeResponse res;
	if (req->templateid_size() == 0)
	{
		for (auto node : m_mapUUIDToNodeItem)
		{
			node.second.ToProtobuf(res.add_nodeinfos());
		}
	}
	else
	{
		auto id_list = req->templateid();
		for (auto id : id_list)
		{
			for (auto servID : m_mapIDToTemplateItem[id].Created)
			{
				m_mapUUIDToNodeItem[servID].ToProtobuf(res.add_nodeinfos());
			}
		}
	}

	return CResult::Make_OK(ConvertObjToStr(&res));
}

CResult Sloong::CServerManage::StopNodeHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<StopNodeRequest>(req_str);
	if (!req)
		return CResult::Make_Error("Parser message object fialed.");

	auto id = req->nodeid();
	if (!m_mapUUIDToNodeItem.exist(id))
		return CResult::Make_Error("NodeID error, the node no exit.");

	list<uint64_t> l;
	l.push_back(id);
	SendEvent(l, Core::ControlEvent::Stop, nullptr);

	return CResult::Succeed();
}

CResult Sloong::CServerManage::RestartNodeHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<RestartNodeRequest>(req_str);
	if (!req)
		return CResult::Make_Error("Parser message object fialed.");

	auto id = req->nodeid();
	if (!m_mapUUIDToNodeItem.exist(id))
		return CResult::Make_Error("NodeID error, the node no exit.");

	list<uint64_t> l;
	l.push_back(id);
	SendEvent(l, Core::ControlEvent::Restart, nullptr);

	return CResult::Succeed();
}

CResult Sloong::CServerManage::QueryReferenceInfoHandler(const string &req_str, CDataTransPackage *pack)
{
	auto uuid = pack->GetSender();
	if (!m_mapUUIDToNodeItem.exist(uuid))
		return CResult::Make_Error(Helper::Format("The node is no registed. [%llu]", uuid));

	auto id = m_mapUUIDToNodeItem[uuid].TemplateID;

	QueryReferenceInfoResponse res;
	auto references = Helper::split(m_mapIDToTemplateItem[id].ConfiguationObj->modulereference(), ',');
	for (auto ref : references)
	{
		auto ref_id = 0;
		if (!ConvertStrToInt(ref, &ref_id))
			continue;
		auto item = res.add_templateinfos();
		auto tpl = m_mapIDToTemplateItem[ref_id];
		item->set_templateid(tpl.ID);
		item->set_providefunctions(tpl.ConfiguationObj->modulefunctoins());
		for (auto node : tpl.Created)
		{
			m_mapUUIDToNodeItem[node].ToProtobuf(item->add_nodeinfos());
		}
	}

	return CResult::Make_OK(ConvertObjToStr(&res));
}

CResult Sloong::CServerManage::ReportLoadStatusHandler(const string &req_str, CDataTransPackage *pack)
{
	auto req = ConvertStrToObj<ReportLoadStatusRequest>(req_str);
	
	m_pLog->Info(Helper::Format("Node[%s] load status :CPU[%d]Mem[%d]", pack->GetSender(), req->cpuload(), req->memroyused() ));
	
	return CResult(ResultType::Ignore);
}
