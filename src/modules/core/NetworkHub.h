/*** 
 * @Author: Chuanbin Wang - wcb@sloong.com
 * @Date: 2019-11-05 08:59:19
 * @LastEditTime: 2020-08-05 18:38:14
 * @LastEditors: Chuanbin Wang
 * @FilePath: /engine/src/modules/core/NetworkHub.h
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


#pragma once

#include "IObject.h"
#include "export.h"
namespace Sloong
{
    class ConnectSession;
    class CEpollEx;
    class CNetworkHub : IObject
    {
    public:
        CNetworkHub();
        ~CNetworkHub();

        CResult Initialize(IControl *);

        void EnableClientCheck(const string &, int);
        void EnableTimeoutCheck(int, int);
        void EnableSSL(const string &, const string &, const string &);

        // event handler
        void Run(SharedEvent);
        void Exit(SharedEvent);
        void SendPackageEventHandler(SharedEvent);
        void OnConnectionBreakedEventHandler(SharedEvent);
        void MonitorSendStatusEventHandler(SharedEvent);
        void RegisteConnectionEventHandler(SharedEvent);
        void OnGetConnectionInfoEventHandler(SharedEvent);

        inline void RegisterProcesser(RequestPackageProcessFunction req, ResponsePackageProcessFunction res, EventPackageProcessFunction event)
        {
            m_pRequestFunc = req;
            m_pResponseFunc = res;
            m_pEventFunc = event;
        }
        inline void RegisterEnvCreateProcesser(CreateProcessEnvironmentFunction value)
        {
            m_pCreateEnvFunc = value;
        }

        /**
         * @Description: In default case, the connect just accept and add to epoll watch list. 
         *       if want do other operation, call this function and set the process, when accpet ent, will call this function .
         * @Params: the function bind for processer
         * @Return: NO
         */
        void RegisterAccpetConnectProcesser(NewConnectAcceptProcessFunction value)
        {
            m_pAcceptFunc = value;
        }

        // Work thread.
        void CheckTimeoutWorkLoop();
        void MessageProcessWorkLoop();

        // Callback function
        ResultType OnNewAccept(int64_t);
        ResultType OnDataCanReceive(int64_t);
        ResultType OnCanWriteData(int64_t);
        ResultType OnOtherEventHappened(int64_t);

    protected:
        void SendConnectionBreak(int64_t);
        void AddMessageToSendList(UniquePackage);

    protected:
        map_ex<int64_t, unique_ptr<ConnectSession>> m_mapConnectIDToSession;
        mutex m_oSockListMutex;
        RUN_STATUS m_emStatus = RUN_STATUS::Created;
        unique_ptr<CEpollEx> m_pEpoll;
        EasySync m_oCheckTimeoutThreadSync;

        LPVOID m_pCTX = nullptr;
        GLOBAL_CONFIG *m_pConfig = nullptr;

        // Timeout check
        int m_nConnectTimeoutTime = 0;
        int m_nCheckTimeoutInterval = 0;
        // Client check
        string m_strClientCheckKey = "";
        int m_nClientCheckKeyLength = 0;
        int m_nClientCheckTime = 0;
        // For message process
        EasySync m_oProcessThreadSync;
        CreateProcessEnvironmentFunction m_pCreateEnvFunc = nullptr;
        RequestPackageProcessFunction m_pRequestFunc = nullptr;
        ResponsePackageProcessFunction m_pResponseFunc = nullptr;
        EventPackageProcessFunction m_pEventFunc = nullptr;
        NewConnectAcceptProcessFunction m_pAcceptFunc = nullptr;
        queue_ex<UniquePackage> *m_pWaitProcessList;
    };
} // namespace Sloong
