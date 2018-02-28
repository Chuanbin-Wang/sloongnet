#pragma once

#include "main.h"

template<typename T> inline
T TYPE_TRANS(LPVOID p)
{
	T tmp = static_cast<T>(p);
	assert(tmp);
	return tmp;
}



typedef enum g_DataCenter_MsgType
{
	ProgramStart,
	ProgramExit,

	//////////////////////////////////////////////////////////////////////////
	// �� * EPollEx * ģ���ṩ����Ϣ
	//////////////////////////////////////////////////////////////////////////
	// �����յ���Ϣ��֮�󣬻ᷢ�͸���Ϣ
	ReveivePackage,

	// �����ӹر�ʱ�ᷢ�͸���Ϣ
	// ��Ҫ�ڴ�����ɺ���ûص����������������Ϣ��
	// ��������ΪCNetworkEvent
	SocketClose,

	// ��Ҫ�������ݸ��ͻ���ʱ��ʹ�ø���Ϣ
	SendMessage,

	//////////////////////////////////////////////////////////////////////////
	// �� * LuaProcessCenter * �ṩ����Ϣ
	//////////////////////////////////////////////////////////////////////////
	// �������lua��������Ϣ��
	// �ڴ�����Ͻ�����ûص��������ص�����Ϊ
	ProcessMessage,
	
	// ������������Lua����
	// ����Ҫ��������Lua Context��ʱ���͸�����
	// ��������ΪCNormalEvent
	ReloadLuaContext,
	

}MSG_TYPE;

struct MySQLConnectInfo
{
	bool Enable;
	string Address;
	int Port;
	string User;
	string Password;
	string Database;
};

struct LuaScriptConfigInfo
{
	string EntryFile;
	string EntryFunction;
	string ProcessFunction;
	string SocketCloseFunction;
	string ScriptFolder;
};

struct LogConfigInfo
{
	bool	DebugMode;
	bool	ShowSendMessage;
	bool	ShowReceiveMessage;
	bool	LogWriteToOneFile;
	bool	ShowSQLCmd;
	bool	ShowSQLResult;
	int		LogLevel;
	string	LogPath;
	int		NetworkPort;
};

struct HandlerItem
{
	void* object;
	LPCALLBACK2FUNC handler;
};

typedef struct _stRecvInfo
{
	long long nSwiftNumber = -1;
	string strMD5 = "";
	string strMessage = "";
}RECVINFO;
