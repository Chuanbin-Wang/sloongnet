#ifndef SLOONGNET_PROCESS_SERVICE_H
#define SLOONGNET_PROCESS_SERVICE_H


#include "sockinfo.h"
#include "core.h"
#include "export.h"
#include <jsoncpp/json/json.h>
#include "LuaProcessCenter.h"

extern "C" {
	CResult MessagePackageProcesser(CDataTransPackage*);
	CResult NewConnectAcceptProcesser(CSockInfo*);
	CResult ModuleInitialization(GLOBAL_CONFIG*);
	CResult ModuleInitialized(IControl*);
}


namespace Sloong
{
	typedef std::function<bool(Functions, string, CDataTransPackage*)> FuncHandler;
	class SloongNetProcess
	{
	public:
		SloongNetProcess() {}
		~SloongNetProcess() {}

		CResult Initialization(GLOBAL_CONFIG*);
		CResult Initialized(IControl*);

		CResult MessagePackageProcesser(CDataTransPackage*);

		bool ProcessMessageHanlder(Functions func, string sender, CDataTransPackage* pack);
		void OnSocketClose(SmartEvent evt);
		
		void RegistFunctionHandler(Functions func, FuncHandler handler);
	protected:
		unique_ptr<CLuaProcessCenter> m_pProcess = make_unique<CLuaProcessCenter>();
		map<string, unique_ptr<CLuaPacket>> m_mapUserInfoList;

		map_ex<Functions, FuncHandler> m_oFunctionHandles;
		IControl* 	m_pControl = nullptr;
		CLog*		m_pLog =nullptr;

		int 		m_nSerialNumber = 0;
		
		GLOBAL_CONFIG* m_pConfig;
		Json::Value    m_oExConfig;
	public:
		static unique_ptr<SloongNetProcess> Instance;
	};

}


#endif