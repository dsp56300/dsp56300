#pragma once

#include <array>
#include <condition_variable>
#include <atomic>
#include <mutex>

namespace dsp56k
{
	class TriggerCv
	{
	public:
		struct Storage
		{
			std::condition_variable cv;
			std::mutex mutex;
			std::atomic_int flag = 0;
		};
	};

	class TriggerWin8
	{
	public:
		struct Storage
		{
			std::atomic_int flag{0};
		};
	};

	class TriggerDefault
	{
	public:
		TriggerDefault() = default;

		void notify();

		void wait();

	private:
		std::condition_variable m_cv;
		std::mutex m_mutex;
		std::atomic_int m_flag{0};
	};

#ifdef _WIN32
	class Trigger
	{
	public:
		using FuncNotify = void(*)(void*);
		using FuncWait = void(*)(void*);

		Trigger();

		Trigger(const Trigger&) = delete;
		Trigger(Trigger&&) = delete;

		~Trigger();

		Trigger& operator=(const Trigger&) = delete;
		Trigger& operator=(Trigger&&) = delete;

		void notify()
		{
			m_funcNotify(m_storage.data());
		}
		void wait()
		{
			m_funcWait(m_storage.data());
		}

	private:
		static constexpr size_t StorageSize = std::max(sizeof(TriggerCv::Storage), sizeof(TriggerWin8::Storage));
		std::array<uint8_t, StorageSize> m_storage;
		FuncNotify m_funcNotify;
		FuncWait m_funcWait;
	};
#else
	using Trigger = TriggerDefault;
#endif

} // namespace dsp56k