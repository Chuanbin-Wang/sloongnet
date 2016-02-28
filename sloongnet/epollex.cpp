#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <queue>
#include "epollex.h"
#include "utility.h"
#include <univ/log.h>
#include <univ/univ.h>
#include <univ/threadpool.h>
#include <univ/exception.h>
#include "progressbar.h"
#define MAXRECVBUF 4096
#define MAXBUF MAXRECVBUF+10

using namespace Sloong;
using namespace Sloong::Universal;

CEpollEx::CEpollEx()
{
	m_pSEM = NULL;
	m_bIsRunning = false;
}

CEpollEx::~CEpollEx()
{
}

void on_sigint(int signal)
{
    exit(0);
}

// Initialize the epoll and the thread pool.
int CEpollEx::Initialize(CLog* plog, int licensePort, int nThreadNum, int nPriorityLevel, bool bShowSendMessage /* = false */)
{
    m_bShowSendMessage = bShowSendMessage;
	m_nPriorityLevel = nPriorityLevel;
    m_pLog = plog;
	m_pLog->Log(CUniversal::Format("epollex is initialize.license port is %d", licensePort));
    //SIGPIPE:在reader终止之后写pipe的时候发生
    //SIG_IGN:忽略信号的处理程序
    //SIGCHLD: 进程Terminate或Stop的时候,SIGPIPE会发送给进程的父进程,缺省情况下该Signal会被忽略
    //SIGINT:由Interrupt Key产生,通常是Ctrl+c或者Delete,发送给所有的ForeGroundGroup进程.
    signal(SIGPIPE,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    signal(SIGINT,&on_sigint);

    // 初始化socket
    m_ListenSock=socket(AF_INET,SOCK_STREAM,0);
    int sock_op = 1;
    // SOL_SOCKET:在socket层面设置
    // SO_REUSEADDR:允许套接字和一个已在使用中的地址捆绑
    setsockopt(m_ListenSock,SOL_SOCKET,SO_REUSEADDR,&sock_op,sizeof(sock_op));

    // 初始化地址结构
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(licensePort);

    // 绑定端口
    errno = bind(m_ListenSock,(struct sockaddr*)&address,sizeof(address));
    if( errno == -1 )
        new normal_except(CUniversal::Format("bind to %d field. errno = %d",licensePort,errno));

    // 监听端口,监听队列大小为1024.可修改为SOMAXCONN
    errno = listen(m_ListenSock,1024);
    // 设置socket为非阻塞模式
    SetSocketNonblocking(m_ListenSock);
    // 创建epoll
    m_EpollHandle=epoll_create(65535);
    // 创建epoll事件对象
    CtlEpollEvent(EPOLL_CTL_ADD,m_ListenSock,EPOLLIN|EPOLLET|EPOLLOUT);
	m_bIsRunning = true;
    // Init the thread pool
	CThreadPool::AddWorkThread(WorkLoop, this, nThreadNum);
	
	return true;
}

void CEpollEx::CtlEpollEvent(int opt, int sock, int events)
{
    struct epoll_event ent;
    memset(&ent,0,sizeof(ent));
    ent.data.fd=sock;
    ent.events=events;

    // 设置事件到epoll对象
    epoll_ctl(m_EpollHandle,opt,sock,&ent);
}


// 设置套接字为非阻塞模式
int CEpollEx::SetSocketNonblocking(int socket)
{
    int op;

    op=fcntl(socket,F_GETFL,0);
    fcntl(socket,F_SETFL,op|O_NONBLOCK);

    return op;
}

/*************************************************
* Function: * epoll_loop
* Description: * epoll检测循环
* Input: *
* Output: *
* Others: *
*************************************************/
void* CEpollEx::WorkLoop(void* pParam)
{
	CEpollEx* pThis = (CEpollEx*)pParam;
	pThis->m_pLog->Log("epoll is start work loop.");
	int sockListen = pThis->m_ListenSock;
    int n,i;
    while(pThis->m_bIsRunning)
    {
        // 返回需要处理的事件数
		n = epoll_wait(pThis->m_EpollHandle, pThis->m_Events, 1024, 1000);

        if( n<=0 ) continue;
        
        for(i=0; i<n; ++i)
        {
            int fd =pThis->m_Events[i].data.fd;
			if (fd == sockListen)
			{
				pThis->OnNewAccept();
			}
            else if(pThis->m_Events[i].events&EPOLLIN)
            {
				pThis->OnDataCanReceive(fd);
            }
            else if(pThis->m_Events[i].events&EPOLLOUT)
            {
				pThis->OnCanWriteData(fd);
            }
            else
            {
				pThis->CloseConnect(fd);
            }
        }
    }
    return 0;
}
/*************************************************
* Function: * check_connect_timeout
* Description: * 检测长时间没反应的网络连接，并关闭删除
* Input: *
* Output: *
* Others: *
*************************************************/
void *check_connect_timeout(void* para)
{
    return 0;
}

void CEpollEx::SendMessage(int sock, int nPriority, const string& nSwift, string msg, const char* pExData, int nSize )
{
	// process msg
	string md5 = CUniversal::MD5_Encoding(msg);
	msg = md5 + "|" + nSwift + "|" + msg;
    if( m_bShowSendMessage )
        m_pLog->Log(msg);
   
	long long len = msg.size();
	// if have exdata, directly add to epoll list.
	if (pExData != NULL && nSize > 0)
	{
        long long Exlen = nSize;
		char* pBuf = new char[len + 8 + 8];
        memcpy(pBuf, (void*)&len, 8);
		memcpy(pBuf + 8, msg.c_str(), len);
		memcpy(pBuf + 8 + len, (void*)&Exlen, 8);

		AddToSendList(sock, nPriority, pBuf, len + 8 + 8, 0, pExData, nSize);
        return;
	}
	else
	{
		CSockInfo* info = m_SockList[sock];
        if(!info)
        {
            CloseConnect(sock);
            return;
        }
		// check the send list size. if all empty, try send message directly.
        if( info->m_bIsSendListEmpty == false && !info->m_pPrepareSendList->empty() )
        {
				char* pBuf = new char[len + 8];
                memcpy(pBuf, (void*)&len, 8);
				memcpy(pBuf + 8, msg.c_str(), len);
				AddToSendList(sock, nPriority, pBuf, len + 8, 0, pExData, nSize);
                return;
		}
	}
    
	// if code run here. the all list is empty. and no have exdata. try send message
	char* pBuf = new char[len + 8];
	memcpy(pBuf, (void*)&len, 8);
	memcpy(pBuf + 8, msg.c_str(), len);
	SendMessageEx(sock, nPriority, pBuf, len + 8);
}

bool CEpollEx::SendMessageEx(int sock, int nPriority, const char *pBuf, int nSize)
{
    int nMsgSend = SendEx(sock, pBuf, nSize, 0, true);
    if( nMsgSend != nSize )
    {
        AddToSendList(sock, nPriority, pBuf, nSize, nMsgSend, NULL, 0);
        return false;
    }
    SAFE_DELETE_ARR(pBuf);
    return true;
}

void CEpollEx::AddToSendList(int socket, int nPriority, const char *pBuf, int nSize, int nStart, const char* pExBuf, int nExSize )
{
    if (pBuf == NULL || nSize <= 0 )
        return;

    CSockInfo* info = m_SockList[socket];
    unique_lock<mutex> lck(info->m_oPreSendMutex);
    SENDINFO *si = new SENDINFO();
    si->nSent = nStart;
    si->nSize = nSize;
    si->pSendBuffer = pBuf;
	si->pExBuffer = pExBuf;
	si->nExSize = nExSize;
    PRESENDINFO psi;
    psi.pSendInfo = si;
    psi.nPriorityLevel = nPriority;
    info->m_pPrepareSendList->push(psi);
    info->m_bIsSendListEmpty = false;
    SetSocketNonblocking(socket);
    CtlEpollEvent(EPOLL_CTL_MOD,socket,EPOLLOUT|EPOLLIN|EPOLLET);
}


void Sloong::CEpollEx::CloseConnect(int socket)
{
    CtlEpollEvent(EPOLL_CTL_DEL, socket, EPOLLIN | EPOLLOUT | EPOLLET);
    close(socket);
	CSockInfo* info = m_SockList[socket];
    if( !info )
        return;

    unique_lock<mutex> lsck(info->m_oSendMutex);
    unique_lock<mutex> lrck(info->m_oReadMutex);
    auto key = m_SockList.find(socket);
    if(key == m_SockList.end())
        return;

    m_pLog->Log(CUniversal::Format("close connect:%s.",info->m_Address));


	for (map<int, CSockInfo*>::iterator i = m_SockList.begin(); i != m_SockList.end();)
	{
		if (i->first == socket)
		{
			m_SockList.erase(i);
			break;
		}
		else
		{
			i++;
		}
	}

    lsck.unlock();
    lrck.unlock();
    SAFE_DELETE(info);
}

int Sloong::CEpollEx::SendEx(int sock,const char* buf, int nSize, int nStart, bool eagain /*= false*/)
{	
    int nAllSent = nStart;
    int nSentSize = nStart;
    int nNosendSize = nSize - nStart;
    CProgressBar pbar("", 100, Number);

	while (nNosendSize > 0)
	{
		nSentSize = write(sock, buf + nSize - nNosendSize, nNosendSize);
		// if errno != EAGAIN or again for error and return is -1, return false
		if (nSentSize == -1 ) 
		{
			if ( eagain == true || errno != EAGAIN )
                return nAllSent;
			else
				continue;
		}
		nNosendSize -= nSentSize;
        nAllSent += nSentSize;
        pbar.Update((float)nAllSent/(float)nSize);
	}
    return nAllSent;
}

int Sloong::CEpollEx::RecvEx(int sock, char** buf, int nSize, bool eagain /* = false */)
{
    int nIsRecv = 0;
    int nNoRecv = nSize;
    int nRecv = 0;
    char* pBuf = *buf;
    while (nIsRecv < nSize)
    {
        nRecv = recv(sock, pBuf + nSize - nNoRecv, nNoRecv, 0 );
        if (nRecv < 0 )
        {
            if ( errno != EAGAIN )
                return 0;
            else if ( eagain == true && errno == EAGAIN )
                return -1;
            else
                continue;
        }
        else if( nRecv == 0)
        {
            return 0;
        }
        nNoRecv -= nRecv;
        nIsRecv += nRecv;
    }
    return nIsRecv;
}

void Sloong::CEpollEx::OnNewAccept()
{
	// accept the connect and add it to the list
	int conn_sock = -1;
	while ((conn_sock = accept(m_ListenSock, NULL, NULL)) > 0)
	{
		struct sockaddr_in add;
		int nSize = sizeof(add);
		memset(&add, 0, sizeof(add));
		getpeername(conn_sock, (sockaddr*)&add, (socklen_t*)&nSize);

		time_t tm;
		time(&tm);

		CSockInfo* info = new CSockInfo(m_nPriorityLevel);
		info->m_Address = string(inet_ntoa(add.sin_addr));
		info->m_nPort = add.sin_port;
		info->m_ConnectTime = tm;
		info->m_sock = conn_sock;
		m_SockList[conn_sock] = info;
		m_pLog->Log(CUniversal::Format("accept client:%s.", info->m_Address));
		//将接受的连接添加到Epoll的事件中.
		// Add the recv event to epoll;
		SetSocketNonblocking(conn_sock);
		CtlEpollEvent(EPOLL_CTL_ADD, conn_sock, EPOLLIN | EPOLLET);
	}
	if (conn_sock == -1)
	{
		if (errno == EAGAIN)
			return;
		else
			m_pLog->Log("accept error.");
	}
}

void Sloong::CEpollEx::OnDataCanReceive( int nSocket )
{
	// 已经连接的用户,收到数据,可以开始读入
	int len = sizeof(long long);
	//char dataLeng[sizeof(long long) + 1] = { 0 };
	char* pLen = new char[sizeof(long long) + 1]();//dataLeng;
	bool bLoop = true;
	while (bLoop)
	{
		// 先读取消息长度
		memset(pLen, 0, len + 1);
		int nRecvSize = RecvEx(nSocket, &pLen, len, true);
		if (nRecvSize == 0)
		{
			// 读取错误,将这个连接从监听中移除并关闭连接
			CloseConnect(nSocket);
			break;
		}
		else if (nRecvSize < 0)
		{
			//由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可读在这里就当作是该次事件已处理过。
			break;
		}
		else
		{
			long dtlen = atol(pLen);
			if (dtlen <= 0)
			{
				m_pLog->Log("Receive data length error.");
				continue;
			}
			char* data = new char[dtlen + 1];
			memset(data, 0, dtlen + 1);

			nRecvSize = RecvEx(nSocket, &data, dtlen);//一次性接受所有消息
			if (nRecvSize == 0)
			{
				CloseConnect(nSocket);
				break;
			}

			CSockInfo* info = m_SockList[nSocket];
			unique_lock<mutex> lck(info->m_oReadMutex);
			queue<string>* pList = &info->m_pReadList[0];
			string msg;

			// check the priority level
			if (m_nPriorityLevel != 0)
			{
				if (data[0] > m_nPriorityLevel || data[0] < 0)
				{
					m_pLog->Log(CUniversal::Format("Receive priority level error. the data is %d, the config level is %d. add this message to last list", data[0], m_nPriorityLevel));
					pList = &info->m_pReadList[m_nPriorityLevel - 1];
				}
				else
				{
					pList = &info->m_pReadList[data[0]];
				}
				const char* msgdata = &data[1];
				msg = msgdata;
			}
			else
			{
				pList = &info->m_pReadList[0];
				msg = data;
			}
			// Add the msg to the sock info list
			delete[] data;

			pList->push(msg);
			lck.unlock();

			// Add the sock event to list
			unique_lock<mutex> elck(m_oEventListMutex);
			m_EventSockList.push(nSocket);
			if ( m_pSEM )
				sem_post(m_pSEM);
			elck.unlock();
		}
	}
	SAFE_DELETE_ARR(pLen);
}

void Sloong::CEpollEx::OnCanWriteData(int nSocket)
{
	// 可以写入事件
	CSockInfo* info = m_SockList[nSocket];
	
	ProcessPrepareSendList(info);
	ProcessSendList(info);

	CtlEpollEvent(EPOLL_CTL_MOD, nSocket, EPOLLIN | EPOLLET | EPOLLOUT);
}



void Sloong::CEpollEx::SetSEM(sem_t* pSem)
{
	m_pSEM = pSem;
}

void Sloong::CEpollEx::ProcessPrepareSendList(CSockInfo* info)
{
	if (info == NULL)
		return;

	// progress the prepare send list first
	if (!info->m_pPrepareSendList->empty())
	{
		if (false == info->m_oPreSendMutex.try_lock())
		{
			return;
		}
		unique_lock<mutex> prelck(info->m_oPreSendMutex, std::adopt_lock);
		if (info->m_pPrepareSendList->empty())
		{
			prelck.unlock();
			return;
		}

		// TODO:: in here i think no need lock the send list. just push data.
		while (!info->m_pPrepareSendList->empty())
		{
			PRESENDINFO* psi = &info->m_pPrepareSendList->front();
			info->m_pPrepareSendList->pop();
			info->m_pSendList[psi->nPriorityLevel].push(psi->pSendInfo);
		}

		prelck.unlock();
	}
}

void Sloong::CEpollEx::ProcessSendList(CSockInfo* pInfo)
{
	// when prepare list process done, do send operation.
	if (false == pInfo->m_oSendMutex.try_lock())
	{
		return;
	}
	unique_lock<mutex> lck(pInfo->m_oSendMutex, std::adopt_lock);

	bool bTrySend = true;

	while (bTrySend)
	{
		queue<SENDINFO*>* list = NULL;
		// prev package no send end. find and try send it again.
		if (-1 != pInfo->m_nLastSentTags)
		{
			list = &pInfo->m_pSendList[pInfo->m_nLastSentTags];
		}
		// find next package. 
		else
		{
			for (int i = 0; i < pInfo->m_nPriorityLevel; i++)
			{
				if (pInfo->m_pSendList[i].empty())
					continue;
				else
				{
					list = &pInfo->m_pSendList[i];
				}
			}
		}
		if (list == NULL)
			return;

		// if no find send info, is no need send anything , remove this sock from epoll.'
		SENDINFO* si = NULL;
		while (si == NULL)
		{
			if (!list->empty())
			{
				si = list->front();
				if (si == NULL)
				{
					list->pop();
				}
			}
			else
			{
				// the send list is empty, so no need loop.
				break;
			}
		}

		if ( si == NULL)
		{
			if (pInfo->m_nLastSentTags != -1)
				pInfo->m_nLastSentTags = -1;
			else
			{
				CtlEpollEvent(EPOLL_CTL_MOD, pInfo->m_sock, EPOLLIN | EPOLLET);
				pInfo->m_bIsSendListEmpty = true;
			}
		}

		// try send first.
		if (si->nSent >= si->nSize)
			// Send ex data
			si->nSent = SendEx(pInfo->m_sock, si->pExBuffer, si->nExSize, si->nSent - si->nSize) + si->nSize;
		else
			// send normal data.
			si->nSent = SendEx(pInfo->m_sock, si->pSendBuffer, si->nSize, si->nSent);

		// check send result.
		// send done, remove the is sent data and try send next package.
		if (si->nSent == (si->nSize + si->nExSize))
		{
			m_pLog->Log(CUniversal::Format("package is send done, send data length is:%d. remove from list.", si->nSent));
			list->pop();
			pInfo->m_nLastSentTags = -1;
			SAFE_DELETE_ARR(si->pSendBuffer);
			SAFE_DELETE_ARR(si->pExBuffer);
			SAFE_DELETE(si);
			bTrySend = true;
		}
		// send falied, wait next event.
		else
		{
			bTrySend = false;
		}
	}

	lck.unlock();
}

void Sloong::CEpollEx::Exit()
{
	m_bIsRunning = false;
}


 