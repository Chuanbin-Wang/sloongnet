#pragma once
#include "defines.h"

// �¼��Ľӿ�
// �����漰���ദ�����������ͷű��������һ��ʹ�������ͷ�
// ���Խ��ɿ���ڷ���֮ǰ�����ݴ������ĸ������Զ�����AddRef���������Ӽ�����
// Ҫ�������ڴ���֮���ֶ�����SAFE_RELEASE_EVENT���������ͷš�

#ifndef SAFE_RELEASE_EVENT
#define SAFE_RELEASE_EVENT(p)		{if(NULL != (p)){if(0>=(p)->Release()){delete p;};p = NULL;}}
#endif // !SAFE_RELEASE_EVENT


namespace Sloong
{
	namespace Interface
	{
		class IEvent
		{
		public:
			IEvent() {}
			virtual ~IEvent() {}
			virtual void AddRef(int nNum = 1) {
				m_nRefCount = m_nRefCount + nNum;
			}
			virtual int Release() {
				m_nRefCount--;
				return m_nRefCount;
			}
			virtual LPVOID GetParams() = 0;
			virtual MSG_TYPE GetEvent() = 0;
			// Get the handler object.
			// it is pointer to the event register.
			virtual LPVOID GetHandler() = 0;
			virtual LPCALLBACK2FUNC GetCallbackFunc() = 0;
			virtual LPCALLBACK2FUNC GetProcessingFunc() = 0;
		protected:
			int m_nRefCount = 0;
		};


		template<typename T> inline
		T EVENT_TRANS(IEvent* p)
		{
			T tmp = dynamic_cast<T>(p);
			assert(tmp);
			return tmp;
		}
	}


}
