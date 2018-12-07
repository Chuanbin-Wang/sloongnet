#pragma once

#include <mutex>
#include <condition_variable>
using namespace std;

namespace Sloong
{

	class lSmartSync
	{
	public:
		lSmartSync();
		~lSmartSync() {}

		void wait();
		// ����ֵ��
		//  true ��ʱ�䴥������ 
		//  false ����ʱ���� 
		bool wait_for(int nSecond);
		void notify_one();
		void notify_all();

	protected:
		condition_variable m_oCV;
		mutex m_oMutex;
	};

}
