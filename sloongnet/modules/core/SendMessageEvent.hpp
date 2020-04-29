/*
 * @Author: WCB
 * @Date: 1970-01-01 08:00:00
 * @LastEditors: WCB
 * @LastEditTime: 2020-04-29 17:01:57
 * @Description: file content
 */
#pragma once
#include "NetworkEvent.hpp"
namespace Sloong
{
	class CDataTransPackage;
	namespace Events
	{
		class CSendMessageEvent : public CNetworkEvent
		{
		public:
			CSendMessageEvent(){m_emType = EVENT_TYPE::SendMessage; }
			virtual	~CSendMessageEvent(){}
			
			void SetRequest( SOCKET target, string connect, int serialnumber, int priority = 1, Functions func = Functions::ProcessMessage)
			{
				m_pData = make_shared<DataPackage>();
				m_pData->set_function(func);
				m_pData->set_content(connect);
				m_pData->set_prioritylevel(priority);
				m_pData->set_serialnumber(serialnumber);

				m_nSocketID = target;
			}

			shared_ptr<DataPackage> GetDataPackage()
			{
				return m_pData;
			}
		
		protected:
			int m_nSocketID;
			CLuaPacket* m_pInfo;
			shared_ptr<DataPackage> m_pData;
		};

	}
}
