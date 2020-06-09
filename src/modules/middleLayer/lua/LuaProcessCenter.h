/*
 * @Author: WCB
 * @Date: 2019-04-14 14:41:59
 * @LastEditors: WCB
 * @LastEditTime: 2019-11-06 17:33:54
 * @Description: file content
 */
#pragma once

#define LUA_INT_TYPE LUA_INT_LONG
#include "core.h"
#include "IObject.h"

#include "lua.h"

namespace Sloong
{
	namespace Events
	{
		class CNetworkEvent;
	}
	using namespace Events;
	class CLuaProcessCenter : IObject
	{
	public:
		CLuaProcessCenter(){}
		~CLuaProcessCenter();

		CResult Initialize(IControl* iMsg);
		CResult NewThreadInit();
		CResult InitLua(CLua* pLua, string folder);
		void CloseSocket(CLuaPacket* uinfo);
		SResult MsgProcess( int function, CLuaPacket * pUInfo, const string& msg, const string& extend );
		int GetFreeLuaContext();
		inline void FreeLuaContext(int id){
			m_oFreeLuaContext.push(id);
			m_oSSync.notify_one();
		}
		void HandleError(const string& err);
		void ReloadContext(IEvent* event);
	protected:
		vector<CLua*>	m_pLuaList;
		vector<bool>	m_oReloadList;
		queue_ex<int>		m_oFreeLuaContext;
		CEasySync		m_oSSync;
		Json::Value*	m_pConfig;
	};

}
