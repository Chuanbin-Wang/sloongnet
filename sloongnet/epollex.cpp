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
    memset(&m_Event,0,sizeof(m_Event));
    m_Event.data.fd=m_ListenSock;
    m_Event.events=EPOLLIN|EPOLLET|EPOLLOUT;

    CLog::showLog(INF,boost::format("EpollHanled=%d")%m_EpollHandle);
    // �����¼���epoll����
    epoll_ctl(m_EpollHandle,EPOLL_CTL_ADD,m_ListenSock,&m_Event);

    // �����̳߳�
    return InitThreadPool(nThreadNums);
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
                    CLog::showLog(INF,"one client is accpet.");
                    g_pThis->m_Event.data.fd=accept(g_pThis->m_ListenSock,NULL,NULL);
                    if(g_pThis->m_Event.data.fd>0)
                    {
                        //�����ܵ�������ӵ�Epoll���¼���.
                        g_pThis->SetSocketNonblocking(g_pThis->m_Event.data.fd);
                        g_pThis->m_Event.events=EPOLLIN|EPOLLET|EPOLLOUT;
                        epoll_ctl(g_pThis->m_EpollHandle,EPOLL_CTL_ADD,g_pThis->m_Event.data.fd,&g_pThis->m_Event);
                    }
                    else
                    {
                        if(errno==EAGAIN)
                            break;
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
                            epoll_ctl(g_pThis->m_EpollHandle, EPOLL_CTL_DEL, ProcessSock, &g_pThis->m_Event);
                            close(ProcessSock);
                            break;
                        }
                    }
                    else if(readLen == 0)
                    {
                        // The connect is disconnected.
                        close(ProcessSock);
                        epoll_ctl(g_pThis->m_EpollHandle,EPOLL_CTL_DEL, ProcessSock, &g_pThis->m_Event);
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

                        string msg(data);
                        CLog::showLog(INF,boost::format("data to string is %s")%msg);
                        g_pThis->m_ReadList.push(msg);
                        delete[] data;
                    }
                }
                string res("success");
                long long len = res.size();
                char* temp = new char[res.size()+8+1];
                memcpy(temp,(void*)&len,8);
                memcpy(temp+8,res.c_str(),res.size()+1);
                send(ProcessSock,temp,res.size()+8+1,0);
                delete[] temp;
            }
            else if(g_pThis->m_Events[i].events&EPOLLOUT)
            {
                // ����д���¼�
                // CLog::showLog(INF,"Socket can write.");
                // if ( g_pThis->m_WriteList.size() > 0 )
                // {
                // CLog::showLog(INF,"Read to write message");
                // process read list.
                // string msg = g_pThis->m_WriteList.front();
                // CLog::showLog(INF,boost::format("send message %1%")%msg);
                // g_pThis->m_WriteList.pop();
                // send(ProcessSock,msg.c_str(),msg.size(),0);
                // }
                //
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
