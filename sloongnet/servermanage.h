#ifndef SERVERMANAGE_H
#define SERVERMANAGE_H

#include "main.h"
#include "configuation.h"
#include "DataTransPackage.h"
#include "IData.h"
namespace Sloong
{

	class CServerManage
	{
	public:
		CResult Initialize( const string& uuid);

		bool RegisterServerHandler(Functions func, string sender, SmartPackage pack);
		bool GetConfigTemplateListHandler(Functions func, string sender, SmartPackage pack);
		bool SetServerConfigHandler(Functions func, string sender, SmartPackage pack);
		bool SetServerConfigTemplateHandler(Functions func, string sender, SmartPackage pack);
		bool GetServerConfigHandler(Functions func, string sender, SmartPackage pack);
		bool SetServerToTemplate(Functions func, string sender, SmartPackage pack);

		/// ���ڳ�ʼ��̫�磬�޷���initializeʱ��ȡ��ֻ����control_server�ֶ�����
		void SetLog(CLog* log) { m_pLog = log; }

	private:
		unique_ptr<CConfiguation>	m_pAllConfig = make_unique<CConfiguation>();
		CLog* m_pLog = nullptr;
		map_ex<string, CServerItem>	m_oServerList;
		map_ex<ModuleType, list_ex<string>> m_oServerTypeList;
	};
}
#endif
