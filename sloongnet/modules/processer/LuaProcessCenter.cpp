#include "LuaProcessCenter.h"
#include "globalfunction.h"
#include "IData.h"
using namespace Sloong;

CLog* g_pLog = nullptr;

CLuaProcessCenter::CLuaProcessCenter()
{
	m_pGFunc = make_unique<CGlobalFunction>();
}


CLuaProcessCenter::~CLuaProcessCenter()
{
	size_t nLen = m_pLuaList.size();
	for (size_t i = 0; i < nLen; i++)
	{
		SAFE_DELETE(m_pLuaList[i]);
	}
}

void Sloong::CLuaProcessCenter::Initialize(IControl* iMsg)
{
	IObject::Initialize(iMsg);
	g_pLog = m_pLog;

	m_pGFunc->Initialize(m_iC);
	m_pConfig = IData::GetModuleConfig();

	m_iC->RegisterEvent(EVENT_TYPE::ReloadLuaContext);
	m_iC->RegisterEventHandler(ReloadLuaContext, std::bind(&CLuaProcessCenter::ReloadContext,this,std::placeholders::_1));
	// 主要的循环方式为，根据输入的处理数来初始化指定数量的lua环境。
	// 然后将其加入到可用队列
	// 在处理开始之前根据队列情况拿到某lua环境的id并将其移除出可用队列
	// 在处理完毕之后重新加回到可用队列中。
	// 这里使用处理线程池的数量进行初始化，保证在所有线程都在处理Lua请求时不会因luacontext发生堵塞
	for (int i = 0; i < m_pConfig->operator[]("LuaContextQuantity").asInt(); i++)
		NewThreadInit();

}


void Sloong::CLuaProcessCenter::HandleError(string err)
{
	g_pLog->Error(CUniversal::Format("[Script]:[%s]", err));
}

void Sloong::CLuaProcessCenter::ReloadContext(SmartEvent event)
{
	size_t n = m_pLuaList.size();
	for (size_t i=0;i<n;i++)
	{
		m_oReloadList[i] = true;
	}
}



int Sloong::CLuaProcessCenter::NewThreadInit()
{
	CLua* pLua = new CLua();
	pLua->SetErrorHandle(HandleError);
	pLua->SetScriptFolder(m_pConfig->operator[]("LuaScriptFolder").asString());
	m_pGFunc->RegistFuncToLua(pLua);
	InitLua(pLua, m_pConfig->operator[]("LuaScriptFolder").asString());
	m_pLuaList.push_back(pLua);
	m_oReloadList.push_back(false);
	int id = (int)m_pLuaList.size() - 1;
	m_oFreeLuaContext.push(id);
	return id;
}

void Sloong::CLuaProcessCenter::InitLua(CLua* pLua, string folder)
{
	if (!pLua->RunScript(m_pConfig->operator[]("LuaEntryFile").asString()))
	{
		throw normal_except("Run Script Fialed.");
	}
	char tag = folder[folder.length() - 1];
    if (tag != '/' && tag != '\\')
	{
		folder += '/';
	}
	pLua->RunFunction(m_pConfig->operator[]("LuaEntryFunction").asString(), CUniversal::Format("'%s'", folder));
}

void Sloong::CLuaProcessCenter::CloseSocket(CLuaPacket* uinfo)
{
	// call close function.
	int id = GetFreeLuaContext();
	CLua* pLua = m_pLuaList[id];
	pLua->RunFunction(m_pConfig->operator[]("LuaSocketCloseFunction").asString() , uinfo);
	m_oFreeLuaContext.push(id);
}


string FormatJSONErrorMessage(string code,string message)
{
	return CUniversal::Format("{\"errno\": \"%s\",\"errmsg\" : \"%s\"}", code, message);
}

bool Sloong::CLuaProcessCenter::MsgProcess(CLuaPacket * pUInfo,const string & msg, string & res, char*& exData, int& exSize)
{
	// In process, need add the lua script runtime and call lua to process.
	// In here, just show log to test.
	exData = nullptr;
	exSize = 0;
	// process msg, get the md5 code and the swift number.
	int id = GetFreeLuaContext();
	if ( id < 0 )
	{
		res = FormatJSONErrorMessage("-1","server is busy now. please try again.");
		return true;
	}
	CLua* pLua = m_pLuaList[id];

	if (m_oReloadList[id] == true)
	{
		InitLua(pLua, m_pConfig->operator[]("LuaScriptFolder").asString());
		m_oReloadList[id] = false;
	}

	CLuaPacket creq;
	creq.SetData("json_request_message", msg);
	CLuaPacket cres;
	bool bRes;
	try
	{
		bRes = pLua->RunFunction(m_pConfig->operator[]("LuaProcessFunction").asString(), pUInfo, &creq, &cres);
	}
	catch(...)
	{
		m_oFreeLuaContext.push(id);
		res = FormatJSONErrorMessage("-2","server process error.");
		return false;
	}
	m_oFreeLuaContext.push(id);
	if (bRes)
	{
		res = cres.GetData("json_response_message","");
		string need = cres.GetData("NeedExData","false");
		if ( need == "true"	)
		{
			auto uuid = cres.GetData("ExDataUUID","");
			auto len = cres.GetData("ExDataSize","");
			auto pData = m_iC->GetTemp("SendList" + uuid);
			if (pData == nullptr)
			{
				res = FormatJSONErrorMessage("-2","ExData no saved in DataCenter, The uuid is " + uuid);
				return true;
			}
			else
			{
				char* pBuf = TYPE_TRANS<char*>(pData);
				exData = pBuf;
				exSize = stoi(len);
			}
		}
		return true;
	}
	else
	{// 运行lua脚本失败
		return false;
	}
}
#define LUA_CONTEXT_WAIT_SECONDE  10
int Sloong::CLuaProcessCenter::GetFreeLuaContext()
{
	
	for ( int i = 0; i<LUA_CONTEXT_WAIT_SECONDE&&m_oFreeLuaContext.empty(); i++)
	{
		m_pLog->Debug("Wait lua context 1 sencond :"+CUniversal::ntos(i));
		m_oSSync.wait_for(1);
	}

	unique_lock<mutex> lck(m_oLuaContextMutex);
	if (m_oFreeLuaContext.empty())
	{
		m_pLog->Debug("no free context");
		return -1;
	}	
	int nID = m_oFreeLuaContext.front();
	m_oFreeLuaContext.pop();
	lck.unlock();
	return nID;
}

