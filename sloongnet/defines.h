#ifndef SLOONGNET_DEFINES_H
#define SLOONGNET_DEFINES_H

// univ head file
#include "univ/defines.h"
#include "univ/univ.h"
#include "univ/log.h"
#include "univ/exception.h"
#include "univ/threadpool.h"
#include "univ/hash.h"
#include "univ/lua.h"
#include "univ/luapacket.h"
using namespace Sloong;
using namespace Sloong::Universal;


template<typename T> inline
T TYPE_TRANS(LPVOID p)
{
	T tmp = static_cast<T>(p);
	assert(tmp);
	return tmp;
}


const int s_llLen = 8;
const int s_lLen = 4;
const int s_PriorityLevel = 5;

enum MessageFunction
{
	/* Sender: All
	   Processer: Control
	   Response: Content(string) - the uuid for sender.
	*/
	RegisteServer,

	/* Sender: ContrulUI
	   Processer: Control
	   Response: Content(JSON) - wait config list. format: 
			{
			  "WaitConfigList": [
				{
				  "UUID": "",
				  "IP": ""
				}
			  ]
			}
	*/
	GetWaitConfigList,

	/*
		Flow: ControlUI -> Control
		Request
	*/
	GetConfigTemplateList,

	SetServerConfigTemplate,

	SetServerToTemplate,

	/* Sender: ControlUI
	   Processer: Control
	   Request: Content - Target server UUID
                Extend - Target server config
	   Response: Result
	*/
	SetServerConfig,



	/* Sender: All 
	   Processer: Control
	   Response: ExtendData(bytes) - config data is not UTF8 string. cannot save in context field.
	   Note: Get the general server config data. the data type is GLOBAL_CONFIG.
	*/
	GetServerConfig,


	// Sender: Process 
	// Processer: Control
	// Response: No
	// Note: if the socket uuid is registed, the control will send error message 
	UserLogin,

	/* Sender: Procxy
	   Processer : Process 
	   Response data: Content - result with JSON string. 
	*/
	ProcessMessage,
	

};

typedef enum g_DataCenter_Event_Type
{
	ProgramStart,
	ProgramExit,

	//////////////////////////////////////////////////////////////////////////
	// 由 * NetworkHub * 模块提供的消息
	//////////////////////////////////////////////////////////////////////////
	// 当连接关闭时会发送该消息
	// 需要在处理完成后调用回调函数以清除连接信息。
	// 参数类型为CNetworkEvent
	SocketClose,

	// 需要发送数据给客户端时，使用该消息
	SendMessage,

	// 需要监听socket的可写状态时，使用该消息
	// 类型为CNetworkEvent.
	MonitorSendStatus,

	
	// 请求重新载入Lua环境
	// 当需要重新载入Lua Context的时候发送该请求。
	// 请求类型为CNormalEvent
	ReloadLuaContext,
	
	

}EVENT_TYPE;


enum RecvStatus
{
	Wait=0,
	Receiving=1,
	Saveing=2,
	Done=3,
	VerificationError=4,
	OtherError=5,
};

// Receive file struce for GFunc
struct RecvDataPackage
{
	string strMD5 = "";
	RecvStatus emStatus = RecvStatus::Wait;
	string strName = "";
	string strPath = "";
};

enum HashType
{
	MD5 = 0,
	SHA_1 = 1,
	SHA_256 = 2,
	SHA_512 = 3,
};

typedef enum g_em_DataItem
{
	ServerConfiguation,
	Logger,
}DATA_ITEM;


#endif