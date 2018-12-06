#ifndef SLOONGWALLUS_H
#define SLOONGWALLUS_H

#include <mutex>
#include <condition_variable>
#include "IEvent.h"

namespace Sloong
{
	namespace Universal
	{
		class CThreadPool;
		class CLuaPacket;
	}
	using namespace Universal;
	using namespace Interface;

	class CServerConfig;
	class CControlCenter;
	class CDataCenter;
	class CMessageCenter;
	class SloongWallUS
	{
	public:
		SloongWallUS();
		~SloongWallUS();

		void Initialize(CServerConfig* config);
		void Run();
		void Exit();
		
		void EventHandler(SmartEvent event);
		
	protected:
		int m_sockServ;
		int* m_sockClient;
		CControlCenter* m_pCC;
		CDataCenter* m_pDC;
		CMessageCenter* m_pMC;
		CLog* m_pLog;
        mutex m_oExitEventMutex;
		condition_variable m_oExitEventCV;
		bool	m_bIsRunning;
	};

}



#endif //SLOONGWALLUS_H
