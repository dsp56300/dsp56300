#include "trigger.h"

#include "logging.h"
#include "win8sync.h"

namespace dsp56k
{
	namespace
	{
		void triggerNotifyCpp11(void* _s)
		{
			auto* s = static_cast<TriggerCv::Storage*>(_s);
			{
				std::unique_lock lock(s->mutex);
				s->flag.store(1, std::memory_order_release);
			}
			s->cv.notify_one();
		}
		void triggerWaitCpp11(void* _s)
		{
			auto* s = static_cast<TriggerCv::Storage*>(_s);

			if (s->flag.exchange(0, std::memory_order_acquire))
				return;

			std::unique_lock lock(s->mutex);

			s->cv.wait(lock, [s]
			{
				return s->flag.exchange(0, std::memory_order_acquire);
			});
		}

#ifdef _WIN32
		void triggerNotifyWin8(void* _s)
		{
			auto* s = static_cast<TriggerWin8::Storage*>(_s);
			s->flag.store(1, std::memory_order_release);
			Win8Sync::getWakeOneFunc()(&s->flag);
		}

		void triggerWaitWin8(void* _s)
		{
			auto* s = static_cast<TriggerWin8::Storage*>(_s);

			while (!s->flag.exchange(0, std::memory_order_acquire))
			{
				int zero=0;
				Win8Sync::getWaitOnAddressFunc()(&s->flag, &zero, sizeof(s->flag), 0xffffffff);
			}
		}
#endif
	}

	void TriggerDefault::notify()
	{
		{
			std::lock_guard lock(m_mutex);
			m_flag.store(1, std::memory_order_release);
		}
		m_cv.notify_one();
	}

	void TriggerDefault::wait()
	{
		if (m_flag.exchange(0, std::memory_order_acquire))
			return;

		std::unique_lock lock(m_mutex);

		m_cv.wait(lock, [this]
		{
			return m_flag.exchange(0, std::memory_order_acquire);
		});
	}

#ifdef _WIN32
	Trigger::Trigger()
	{
		if (Win8Sync::isSupported())
		{
			m_funcNotify = triggerNotifyWin8;
			m_funcWait = triggerWaitWin8;
			new (m_storage.data()) TriggerWin8::Storage();
		}
		else
		{
			m_funcNotify = triggerNotifyCpp11;
			m_funcWait = triggerWaitCpp11;
			new (m_storage.data()) TriggerCv::Storage();
		}
	}

	Trigger::~Trigger()
	{
		if (Win8Sync::isSupported())
			reinterpret_cast<TriggerWin8::Storage*>(m_storage.data())->~Storage();
		else
			reinterpret_cast<TriggerCv::Storage*>(m_storage.data())->~Storage();
	}
#endif
}
