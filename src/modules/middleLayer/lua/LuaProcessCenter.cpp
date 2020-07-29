/*** 
 * @Author: Chuanbin Wang
 * @Date: 2019-04-14 14:41:59
 * @LastEditTime: 2020-07-29 19:37:55
 * @LastEditors: Chuanbin Wang
 * @FilePath: /engine/src/modules/middleLayer/lua/LuaProcessCenter.cpp
 * @Copyright 2015-2020 Sloong.com. All Rights Reserved
 * @Description: 
 */
/*** 
 * @......................................&&.........................
 * @....................................&&&..........................
 * @.................................&&&&............................
 * @...............................&&&&..............................
 * @.............................&&&&&&..............................
 * @...........................&&&&&&....&&&..&&&&&&&&&&&&&&&........
 * @..................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&..............
 * @................&...&&&&&&&&&&&&&&&&&&&&&&&&&&&&.................
 * @.......................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.........
 * @...................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&...............
 * @..................&&&   &&&&&&&&&&&&&&&&&&&&&&&&&&&&&............
 * @...............&&&&&@  &&&&&&&&&&..&&&&&&&&&&&&&&&&&&&...........
 * @..............&&&&&&&&&&&&&&&.&&....&&&&&&&&&&&&&..&&&&&.........
 * @..........&&&&&&&&&&&&&&&&&&...&.....&&&&&&&&&&&&&...&&&&........
 * @........&&&&&&&&&&&&&&&&&&&.........&&&&&&&&&&&&&&&....&&&.......
 * @.......&&&&&&&&.....................&&&&&&&&&&&&&&&&.....&&......
 * @........&&&&&.....................&&&&&&&&&&&&&&&&&&.............
 * @..........&...................&&&&&&&&&&&&&&&&&&&&&&&............
 * @................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&............
 * @..................&&&&&&&&&&&&&&&&&&&&&&&&&&&&..&&&&&............
 * @..............&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&....&&&&&............
 * @...........&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&......&&&&............
 * @.........&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.........&&&&............
 * @.......&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&...........&&&&............
 * @......&&&&&&&&&&&&&&&&&&&...&&&&&&...............&&&.............
 * @.....&&&&&&&&&&&&&&&&............................&&..............
 * @....&&&&&&&&&&&&&&&.................&&...........................
 * @...&&&&&&&&&&&&&&&.....................&&&&......................
 * @...&&&&&&&&&&.&&&........................&&&&&...................
 * @..&&&&&&&&&&&..&&..........................&&&&&&&...............
 * @..&&&&&&&&&&&&...&............&&&.....&&&&...&&&&&&&.............
 * @..&&&&&&&&&&&&&.................&&&.....&&&&&&&&&&&&&&...........
 * @..&&&&&&&&&&&&&&&&..............&&&&&&&&&&&&&&&&&&&&&&&&.........
 * @..&&.&&&&&&&&&&&&&&&&&.........&&&&&&&&&&&&&&&&&&&&&&&&&&&.......
 * @...&&..&&&&&&&&&&&&.........&&&&&&&&&&&&&&&&...&&&&&&&&&&&&......
 * @....&..&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&...........&&&&&&&&.....
 * @.......&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&..............&&&&&&&....
 * @.......&&&&&.&&&&&&&&&&&&&&&&&&..&&&&&&&&...&..........&&&&&&....
 * @........&&&.....&&&&&&&&&&&&&.....&&&&&&&&&&...........&..&&&&...
 * @.......&&&........&&&.&&&&&&&&&.....&&&&&.................&&&&...
 * @.......&&&...............&&&&&&&.......&&&&&&&&............&&&...
 * @........&&...................&&&&&&.........................&&&..
 * @.........&.....................&&&&........................&&....
 * @...............................&&&.......................&&......
 * @................................&&......................&&.......
 * @.................................&&..............................
 * @..................................&..............................
 */

#include "LuaProcessCenter.h"
#include "globalfunction.h"
#include "IData.h"
using namespace Sloong;

CResult Sloong::CLuaProcessCenter::Initialize(IControl *iMsg)
{
	IObject::Initialize(iMsg);

	m_pConfig = IData::GetModuleConfig();

	// 主要的循环方式为，根据输入的处理数来初始化指定数量的lua环境。
	// 然后将其加入到可用队列
	// 在处理开始之前根据队列情况拿到某lua环境的id并将其移除出可用队列
	// 在处理完毕之后重新加回到可用队列中。
	// 这里使用处理线程池的数量进行初始化，保证在所有线程都在处理Lua请求时不会因luacontext发生堵塞
	auto num = m_pConfig->operator[]("LuaContextQuantity").asInt();
	if (num < 1)
		return CResult::Make_Error("LuaContextQuantity must be bigger than 0,");

	for (int i = 0; i < num; i++)
	{
		auto res = NewThreadInit();
		if (res.IsFialed())
			return res;
	}
	return CResult::Succeed;
}

void Sloong::CLuaProcessCenter::HandleError(const string &err)
{
	m_pLog->Error(Helper::Format("[Script]:[%s]", err.c_str()));
}

void Sloong::CLuaProcessCenter::ReloadContext()
{
	for (auto &i : m_listLuaContent)
	{
		i.Reload = true;
	}
}

CResult Sloong::CLuaProcessCenter::NewThreadInit()
{
	LuaContent lua;
	lua.Content = new CLua();
	lua.Reload = false;
	lua.Content->SetErrorHandle(std::bind(&CLuaProcessCenter::HandleError, this, placeholders::_1));
	lua.Content->SetScriptFolder(m_pConfig->operator[]("LuaScriptFolder").asString());
	CGlobalFunction::Instance->RegistFuncToLua(lua.Content);
	auto res = InitLua(lua.Content, m_pConfig->operator[]("LuaScriptFolder").asString());
	if (res.IsFialed())
		return res;
	
	m_listLuaContent.push_back(lua);
	int id = (int)m_listLuaContent.size() - 1;
	FreeLuaContext(id);
	return CResult::Succeed;
}

CResult Sloong::CLuaProcessCenter::InitLua(CLua *pLua, string folder)
{
	if (!pLua->RunScript(m_pConfig->operator[]("LuaEntryFile").asString()))
	{
		return CResult::Make_Error("Run Script Fialed.");
	}
	if (!pLua->RunFunction(m_pConfig->operator[]("LuaEntryFunction").asString(), Helper::Format("'%s'", folder.c_str())))
	{
		return CResult::Make_Error("Run Function Fialed.");
	}
	return CResult::Succeed;
}

void Sloong::CLuaProcessCenter::CloseSocket(CLuaPacket *uinfo)
{
	// call close function.
	int id = GetFreeLuaContext();
	auto pLua = m_listLuaContent[id].Content;
	pLua->RunFunction(m_pConfig->operator[]("LuaSocketCloseFunction").asString(), uinfo, 0, "", "");
	FreeLuaContext(id);
}

SResult Sloong::CLuaProcessCenter::MsgProcess(int function, CLuaPacket *pUInfo, const string &msg, const string &extend)
{
	int id = GetFreeLuaContext();
	if (id < 0)
		return SResult::Make_Error("server is busy now. please try again.");
	try
	{
		auto &content = m_listLuaContent[id];
		if (true == content.Reload)
		{
			InitLua(content.Content, m_pConfig->operator[]("LuaScriptFolder").asString());
			content.Reload = false;
		}
		string extendUUID("");
		auto res = content.Content->RunFunction(m_pConfig->operator[]("LuaProcessFunction").asString(), pUInfo, function, msg, extend, &extendUUID);
		FreeLuaContext(id);
		if (res.IsFialed())
			return SResult::Make_Error(res.GetMessage());
		else
			return SResult::Make_OK(extendUUID, res.GetMessage());
	}
	catch (const exception &ex)
	{
		FreeLuaContext(id);
		return SResult::Make_Error("server process error." + string(ex.what()));
	}
	catch (...)
	{
		FreeLuaContext(id);
		return SResult::Make_Error("server process error.");
	}
}
#define LUA_CONTEXT_WAIT_SECONDE 10
int Sloong::CLuaProcessCenter::GetFreeLuaContext()
{
	for (int i = 0; i < LUA_CONTEXT_WAIT_SECONDE && m_oFreeLuaContext.empty(); i++)
	{
		m_pLog->Debug("Wait lua context 1 sencond :" + Helper::ntos(i));
		m_oSSync.wait_for(500);
	}

	if (m_oFreeLuaContext.empty())
	{
		m_pLog->Debug("no free context");
		return -1;
	}
	int nID = m_oFreeLuaContext.front();
	m_oFreeLuaContext.pop();
	return nID;
}
