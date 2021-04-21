#pragma once

#include "dsp.h"

namespace dsp56k
{
	class DSP;

	class DSPThread final
	{
	public:
		using Guard = std::lock_guard<std::mutex>;

		explicit DSPThread(DSP& _dsp);
		~DSPThread();
		void join();
		std::mutex& mutex() { return m_mutex; }

	private:
		void threadFunc();

		DSP& m_dsp;

		std::mutex m_mutex;
		std::unique_ptr<std::thread> m_thread;

		std::atomic<bool> m_runThread;
		uint64_t m_ips = 0;
	};
}
