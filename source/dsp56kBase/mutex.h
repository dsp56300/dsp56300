#pragma once

#ifdef _WIN32
#	define NOMINMAX
#	define WIN32_LEAN_AND_MEAN
#	define NOGDI
#	define NOSERVICE
#	include <Windows.h>
#else
#	include <pthread.h>
#endif

namespace dsp56k
{
	// Lightweight mutex that avoids std::mutex overhead (CRT function call,
	// error checking, exception throwing on MSVC). Compatible with
	// std::lock_guard and std::unique_lock (provides lock/unlock).
	// On Windows uses SRWLOCK (available since Vista, single atomic op
	// uncontended). On POSIX uses pthread_mutex_t.
	class Mutex
	{
	public:
#ifdef _WIN32
		Mutex() noexcept			{ InitializeSRWLock(&m_lock); }
		void lock() noexcept		{ AcquireSRWLockExclusive(&m_lock); }
		void unlock() noexcept		{ ReleaseSRWLockExclusive(&m_lock); }
		bool try_lock() noexcept	{ return TryAcquireSRWLockExclusive(&m_lock) != 0; }
	private:
		SRWLOCK m_lock{};
#else
		Mutex() noexcept			{ pthread_mutex_init(&m_lock, nullptr); }
		~Mutex() noexcept			{ pthread_mutex_destroy(&m_lock); }
		void lock() noexcept		{ pthread_mutex_lock(&m_lock); }
		void unlock() noexcept		{ pthread_mutex_unlock(&m_lock); }
		bool try_lock() noexcept	{ return pthread_mutex_trylock(&m_lock) == 0; }
	private:
		pthread_mutex_t m_lock{};
#endif
	public:
		Mutex(const Mutex&) = delete;
		Mutex& operator=(const Mutex&) = delete;
	};
}
