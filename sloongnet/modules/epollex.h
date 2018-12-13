#ifndef CEPOLLEX_H
#define CEPOLLEX_H

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <openssl/ssl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h> 

#include "IObject.h"
typedef unsigned char byte;

namespace Sloong
{
	namespace Universal
	{
		class CLog;
	}
	using namespace Universal;
	using namespace Interface;

	
	typedef std::function<NetworkResult(int)> EpollEventHandlerFunc;
	
	class CSockInfo;
	class CServerConfig;
	class CEpollEx : IObject
	{
	public:
        CEpollEx();
		virtual ~CEpollEx();
        void Initialize(IMessage* iM,IData* iData);
		void Run();
		
		void SetLogConfiguration(bool bShowSendMessage, bool bShowReceiveMessage);
		void Exit();

		void SetEventHandler(EpollEventHandlerFunc,EpollEventHandlerFunc,EpollEventHandlerFunc,EpollEventHandlerFunc);
        
		/// 修改socket的epoll监听事件
		void CtlEpollEvent(int opt, int sock, int events);

	protected:
		/// 设置socket到非阻塞模式
		int SetSocketNonblocking(int socket);
		
		// event function
		void CloseConnectEventHandler(SmartEvent event);

		void MainWorkLoop(SMARTER params);

	protected:
		EpollEventHandlerFunc OnDataCanReceive = nullptr;
		EpollEventHandlerFunc OnNewAccept = nullptr;
		EpollEventHandlerFunc OnCanWriteData = nullptr;
		EpollEventHandlerFunc OnOtherEventHappened = nullptr;
		
	protected:
		int     m_ListenSock;
		int 	m_EpollHandle;
		//struct epoll_event m_Event;
		epoll_event m_Events[1024];
	public:
		
		bool m_bIsRunning;
		CServerConfig* m_pConfig;
	};
}


#endif // CEPOLLEX_H
