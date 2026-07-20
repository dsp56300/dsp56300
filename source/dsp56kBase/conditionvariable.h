#pragma once

#include <array>
#include <condition_variable>
#include <atomic>
#include <cstdint>

namespace dsp56k
{
	class ConditionVariableCpp11
	{
	public:
		struct Storage
		{
			std::condition_variable cv;
		};
	};

	class ConditionVariableWin8
	{
	public:
		struct Storage
		{
			Storage() = default;
			std::atomic_int m_waiters{0};
		};
	};

#ifdef _WIN32
	class ConditionVariable
	{
	public:
		using FuncNotifyAll = void(*)(void*);
		using FuncNotifyOne = void(*)(void*);
		using FuncWait = void(*)(void*, std::unique_lock<std::mutex>&);

		ConditionVariable();
		~ConditionVariable();

		void notify_all()
		{
			m_funcNotifyAll(m_storage.data());
		}
		void notify_one()
		{
			m_funcNotifyOne(m_storage.data());
		}
		void wait(std::unique_lock<std::mutex>& _lock)
		{
			m_funcWait(m_storage.data(), _lock);
		}

		template<typename Pred> void wait(std::unique_lock<std::mutex>& _lock, Pred _pred)
		{
			while (!_pred())
				m_funcWait(m_storage.data(), _lock);
		}

	private:
		static constexpr size_t StorageSize = std::max(sizeof(ConditionVariableCpp11::Storage), sizeof(ConditionVariableWin8::Storage));

		std::array<uint8_t, StorageSize> m_storage;

		FuncWait m_funcWait;
		FuncNotifyOne m_funcNotifyOne;
		FuncNotifyAll m_funcNotifyAll;
	};
#elif defined(__APPLE__)
	// macOS fast condition variable built on the __ulock futex - the direct counterpart of the Win8
	// WaitOnAddress path above. std::condition_variable on Darwin turns every notify/wait into a
	// __psynch_cvsignal / __psynch_cvwait kernel syscall; the ring buffers wake a waiter on every
	// frame, so at boot a single feeder waking 9 DSP threads one frame at a time collapses into a
	// syscall storm. This keeps the common (already-signalled) case in userspace like Windows does.
	class ConditionVariable
	{
	public:
		void notify_all();
		void notify_one();
		void wait(std::unique_lock<std::mutex>& _lock);

		template<typename Pred> void wait(std::unique_lock<std::mutex>& _lock, Pred _pred)
		{
			while (!_pred())
				wait(_lock);
		}

	private:
		std::atomic<uint32_t> m_seq{0};	// futex word; every notify bumps it, wait blocks while it is unchanged
	};
#else
	using ConditionVariable = std::condition_variable;
#endif
}
