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
#include "univ/log.h"
#include "univ/univ.h"
#define MAXRECVBUF 4096
#define MAXBUF MAXRECVBUF+10

CEpollEx* CEpollEx::g_pThis = NULL;

CEpollEx::CEpollEx()
{
    g_pThis = this;
    CLog::showLog(INF,"epollex is build.");
}

CEpollEx::~CEpollEx()
{
}

void on_sigint(int signal)
{
    exit(0);
}

// Initialize the epoll and the thread pool.
int CEpollEx::Initialize(int nThreadNums, int licensePort)
{
    CLog::showLog(INF,boost::format("epollex is initialize.license port is %d")%licensePort);
    //SIGPIPE:��reader��ֹ֮��дpipe��ʱ����
    //SIG_IGN:�����źŵĴ������
    //SIGCHLD: ����Terminate��Stop��ʱ��,SIGPIPE�ᷢ�͸����̵ĸ�����,ȱʡ����¸�Signal�ᱻ����
    //SIGINT:��Interrupt Key����,ͨ����Ctrl+c����Delete,���͸����е�ForeGroundGroup����.
    signal(SIGPIPE,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);
    signal(SIGINT,&on_sigint);

    // ��ʼ��socket
    m_ListenSock=socket(AF_INET,SOCK_STREAM,0);
    int sock_op = 1;
    // SOL_SOCKET:��socket��������
    // SO_REUSEADDR:�����׽��ֺ�һ������ʹ���еĵ�ַ����
    setsockopt(m_ListenSock,SOL_SOCKET,SO_REUSEADDR,&sock_op,sizeof(sock_op));

    // ��ʼ����ַ�ṹ
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(licensePort);

    // �󶨶˿�
    errno = bind(m_ListenSock,(struct sockaddr*)&address,sizeof(address));
    if( errno == -1 )
        cout<<"bind to "<<licensePort<<" field. errno = "<<errno<<endl;

    // �����˿�,�������д�СΪ1024.���޸�ΪSOMAXCONN
    errno = listen(m_ListenSock,1024);
    // ����socketΪ������ģʽ
    SetSocketNonblocking(m_ListenSock);
    // ����epoll
    m_EpollHandle=epoll_create(65535);
    // ����epoll�¼�����
    CtlEpollEvent(EPOLL_CTL_ADD,m_ListenSock,EPOLLIN|EPOLLET|EPOLLOUT);

    // �����̳߳�
    return InitThreadPool(nThreadNums);
}

void CEpollEx::CtlEpollEvent(int opt, int sock, int events)
{
    struct epoll_event ent;
    memset(&ent,0,sizeof(ent));
    ent.data.fd=sock;
    ent.events=events;

    // �����¼���epoll����
    epoll_ctl(m_EpollHandle,opt,sock,&ent);
}


/*************************************************
* Function: * init_thread_pool
* Description: * ��ʼ���߳�
* Input: * threadNum:���ڴ���epoll���߳���
* Output: *
* Others: * �˺���Ϊ��̬static����,
*************************************************/
int CEpollEx::InitThreadPool(int threadNum)
{
    pthread_t threadId;

    //��ʼ��epoll�̳߳�,
    for ( int i = 0; i < threadNum; i++)
    {
        errno = pthread_create(&threadId, 0, WorkLoop, (void *)0);
        if (errno != 0)
        {
            printf("pthread create failed!\n");
            return(errno);
        }
    }

    // errno = pthread_create(&threadId, 0, check_connect_timeout, (void *)0);

    return 0;
}

// �����׽���Ϊ������ģʽ
int CEpollEx::SetSocketNonblocking(int socket)
{
    int op;

    op=fcntl(socket,F_GETFL,0);
    fcntl(socket,F_SETFL,op|O_NONBLOCK);

    return op;
}

/*************************************************
* Function: * epoll_loop
* Description: * epoll���ѭ��
* Input: *
* Output: *
* Others: *
*************************************************/
void* CEpollEx::WorkLoop(void* para)
{
    CLog::showLog(INF,"epoll is start work loop.");
    int n,i;
    while(true)
    {
        // ������Ҫ������¼���
        n=epoll_wait(g_pThis->m_EpollHandle,g_pThis->m_Events,1024,-1);
        if( n<=0 ) continue;

        for(i=0; i<n; ++i)
        {
            int ProcessSock =g_pThis->m_Events[i].data.fd;
            if(ProcessSock==g_pThis->m_ListenSock)
            {
                // �����ӽ���
                while(true)
                {
                    // accept the connect and add it to the list
                    int conn_sock = -1;
                    while ((conn_sock = accept(g_pThis->m_ListenSock,NULL,NULL)) > 0)
                    {
                        CSockInfo* info = new CSockInfo();
                        struct sockaddr_in add;
                        int nSize = sizeof(add);
                        memset(&add,0,sizeof(add));
                        getpeername(conn_sock, (sockaddr*)&add, (socklen_t*)&nSize );

                        info->m_Address = inet_ntoa(add.sin_addr);
                        info->m_nPort = add.sin_port;
                        time_t tm;
                        time(&tm);
                        info->m_ConnectTime = tm;
                        info->m_sock = conn_sock;
                        g_pThis->m_SockList[conn_sock] = info;
                        CLog::showLog(INF,CUniversal::Format("accept client:%s.",info->m_Address));
                        //�����ܵ�������ӵ�Epoll���¼���.
                        // Add the recv event to epoll;
                        g_pThis->SetSocketNonblocking(conn_sock);
                        g_pThis->CtlEpollEvent(EPOLL_CTL_ADD,conn_sock,EPOLLIN|EPOLLET);
                    }
                    if (conn_sock == -1) {
                        if (errno == EAGAIN )
                            break;
                        else
                            CLog::showLog(INF,"accept error.");

                    }
                }
            }
            else if(g_pThis->m_Events[i].events&EPOLLIN)
            {
                CLog::showLog(INF,"Socket can read.");
                // �Ѿ����ӵ��û�,�յ�����,���Կ�ʼ����
                bool bLoop = true;
                while(bLoop)
                {
                    // �ȶ�ȡ��Ϣ����
                    int len = 8;//sizeof(long long);
                    int readLen;
                    char dataLeng[9] = {0};
                    readLen = recv(ProcessSock,dataLeng,len,0);
                    CLog::showLog(INF,boost::format("recv %d bytes.")%readLen);
                    if(readLen < 0)
                    {
                        //�����Ƿ�������ģʽ,���Ե�errnoΪEAGAINʱ,��ʾ��ǰ�������������ݿ�//��������͵����Ǹô��¼��Ѵ������
                        if(errno == EAGAIN)
                        {
                            printf("EAGAIN\n");
                            break;
                        }
                        else
                        {
                            // ��ȡ����,��������ӴӼ������Ƴ����ر�����
                            printf("recv error!\n");
                            //epoll_ctl(g_pThis->m_EpollHandle, EPOLL_CTL_DEL, ProcessSock, &g_pThis->m_Event);
                            close(ProcessSock);
                            g_pThis->CtlEpollEvent(EPOLL_CTL_DEL,ProcessSock,EPOLLIN|EPOLLOUT|EPOLLET);
                            break;
                        }
                    }
                    else if(readLen == 0)
                    {
                        // The connect is disconnected.
                        close(ProcessSock);
                        g_pThis->CtlEpollEvent(EPOLL_CTL_DEL,ProcessSock,EPOLLIN|EPOLLOUT|EPOLLET);
                        //epoll_ctl(g_pThis->m_EpollHandle,EPOLL_CTL_DEL, ProcessSock, &g_pThis->m_Event);
                        break;
                    }
                    else
                    {
                        long dtlen = atol(dataLeng);
                        CLog::showLog(INF,boost::format("dataLen=%s|%d")%dataLeng%dtlen);
                        dtlen++;
                        char* data = new char[dtlen];
                        memset(data,0,dtlen);

                        readLen = recv(ProcessSock,data,dtlen,0);//һ���Խ���������Ϣ
                        CLog::showLog(INF,boost::format("recv msg:%d|%s")%dtlen%data);

                        if(readLen >= dtlen)
                            bLoop = true; // ��Ҫ�ٴζ�ȡ
                        else
                            bLoop = false;

                        // Add the msg to the sock info list
                        string msg(data);
                        CLog::showLog(INF,boost::format("data to string is %s")%msg);
                        CSockInfo* info = g_pThis->m_SockList[ProcessSock];
                        info->m_ReadList.push(msg);
                        delete[] data;

                        // Add the sock event to list
                        g_pThis->m_EventSockList.push(ProcessSock);
                    }
                }
            }
            else if(g_pThis->m_Events[i].events&EPOLLOUT)
            {
                // ����д���¼�
                CLog::showLog(INF,"Socket can write.");
                CSockInfo* info = g_pThis->m_SockList[ProcessSock];
                while (info->m_WriteList.size())
                {
                    string msg = info->m_WriteList.front();
                    info->m_WriteList.pop();
                    CLog::showLog(INF,boost::format("send message %1%")%msg);
                    if(!SendEx(ProcessSock,msg))
                    {
                        CLog::showLog(ERR,"write error.");
                    }
                }
            }
            else
            {
                //close(g_pThis->m_Events[i].data.fd);
            }
        }
    }
    return 0;
}
/*************************************************
* Function: * check_connect_timeout
* Description: * ��ⳤʱ��û��Ӧ���������ӣ����ر�ɾ��
* Input: *
* Output: *
* Others: *
*************************************************/
void *check_connect_timeout(void* para)
{
    return 0;
}

void CEpollEx::SendMessage(int sock, string msg)
{
    if( false == SendEx(sock,msg,true) )
    {
        CSockInfo* info = m_SockList[sock];
        info->m_WriteList.push(msg);
        // Add to epoll list
        g_pThis->SetSocketNonblocking(sock);
        g_pThis->CtlEpollEvent(EPOLL_CTL_ADD,sock,EPOLLOUT|EPOLLET);
    }
}

bool CEpollEx::SendEx( int sock, string msg, bool eagain /* = false */ )
{
    long long len = msg.size() + 1;
    char* buf = new char[msg.size()+8+1];
    memcpy(buf,(void*)&len,8);
    memcpy(buf+8,msg.c_str(),msg.size()+1);
    //send(ProcessSock,temp,msg.size()+8+1,0);


    int nwrite, data_size = msg.length() +8 +1;
    //const char* buf = msg.c_str();
    int n = data_size;
    while (n > 0) {
        nwrite = write(sock, buf + data_size - n, n);
        if (nwrite < n)
        {
            // if errno != EAGAIN or again for error and return is -1, return false
            if (nwrite == -1 && (errno != EAGAIN  || eagain == true )) {
                delete[] buf;
                return false;
            }
            break;
        }
        n -= nwrite;
    }
    delete[] buf;
    return true;
}
