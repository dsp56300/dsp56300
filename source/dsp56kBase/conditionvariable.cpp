#include "conditionvariable.h"

#include "win8sync.h"

namespace dsp56k
{
	namespace
	{
		void cvCpp11NotifyAll(void* _s)
		{
			auto* s = static_cast<ConditionVariableCpp11::Storage*>(_s);
			s->cv.notify_all();
		}

		void cvCpp11NotifyOne(void* _s)
		{
			auto* s = static_cast<ConditionVariableCpp11::Storage*>(_s);
			s->cv.notify_one();
		}

		void cvCpp11Wait(void* _s, std::unique_lock<std::mutex>& _lock)
		{
			auto* s = static_cast<ConditionVariableCpp11::Storage*>(_s);
			s->cv.wait(_lock);
		}
#ifdef _WIN32
		void cvWin8NotifyAll(void* _s)
		{
			auto* s = static_cast<ConditionVariableWin8::Storage*>(_s);
	        s->m_waiters.fetch_add(1, std::memory_order_relaxed);
			Win8Sync::getWakeAllFunc()(&s->m_waiters);
		}

		void cvWin8NotifyOne(void* _s)
		{
			auto* s = static_cast<ConditionVariableWin8::Storage*>(_s);
	        s->m_waiters.fetch_add(1, std::memory_order_relaxed);
			Win8Sync::getWakeOneFunc()(&s->m_waiters);
		}

		void cvWin8Wait(void* _s, std::unique_lock<std::mutex>& _lock)
		{
			auto* s = static_cast<ConditionVariableWin8::Storage*>(_s);
	        int oldValue = s->m_waiters.load(std::memory_order_relaxed);
	        _lock.unlock();
			Win8Sync::getWaitOnAddressFunc()(&s->m_waiters, &oldValue, sizeof(oldValue), 0xffffffff);
	        _lock.lock();
		}
#endif		
	}

#ifdef _WIN32
	ConditionVariable::ConditionVariable()
	{
		if (Win8Sync::isSupported())
		{
			m_funcNotifyAll = cvWin8NotifyAll;
			m_funcNotifyOne = cvWin8NotifyOne;
			m_funcWait = cvWin8Wait;

			new (m_storage.data()) ConditionVariableWin8::Storage();
		}
		else
		{
			m_funcNotifyAll = cvCpp11NotifyAll;
			m_funcNotifyOne = cvCpp11NotifyOne;
			m_funcWait = cvCpp11Wait;

			new (m_storage.data()) ConditionVariableCpp11::Storage();
		}
	}

	ConditionVariable::~ConditionVariable()
	{
		if (Win8Sync::isSupported())
			reinterpret_cast<ConditionVariableWin8::Storage*>(m_storage.data())->~Storage();
		else
			reinterpret_cast<ConditionVariableCpp11::Storage*>(m_storage.data())->~Storage();
	}
#endif
}
