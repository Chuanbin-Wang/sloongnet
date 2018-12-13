#pragma once

#define LUA_INT_TYPE LUA_INT_LONG

#include <univ/lua.h>
#include "IMessage.h"
#include "IData.h"
#include "SmartSync.h"
namespace Sloong
{
	using namespace Universal;
	using namespace Interface;
	namespace Events
	{
		class CNetworkEvent;
	}
	using namespace Events;
	class CServerConfig;
	class CGlobalFunction;
	class CLuaProcessCenter
	{
	public:
		CLuaProcessCenter();
		~CLuaProcessCenter();

		void Initialize(IMessage* iMsg,IData* iData);
		int NewThreadInit();
		void InitLua(CLua* pLua, string folder);
		void CloseSocket(CLuaPacket* uinfo);
		bool MsgProcess( CLuaPacket * pUInfo, string & msg, string & res, char*& exData, int& exSize);
		int GetFreeLuaContext();
		
		void ReloadContext(SmartEvent event);
	public:
		static void HandleError(string err);
	protected:
		vector<CLua*>	m_pLuaList;
		vector<bool>	m_oReloadList;
		queue<int>		m_oFreeLuaContext;
		static CLog*	m_pLog;
		CSmartSync      m_oSSync;
		mutex			m_oLuaContextMutex;
		IMessage*		m_iMsg;
		IData*			m_iData;
		CServerConfig*	m_pConfig;
		unique_ptr<CGlobalFunction> m_pGFunc;
	};

}
