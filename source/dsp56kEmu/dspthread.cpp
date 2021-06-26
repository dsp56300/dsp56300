#include "dspthread.h"

#include <iostream>

#include "dsp.h"

#ifdef WIN32
#include <Windows.h>
#endif

namespace dsp56k
{
	DSPThread::DSPThread(DSP& _dsp): m_dsp(_dsp), m_runThread(true)
	{
		m_thread.reset(new std::thread([this]
		{
			threadFunc();
		}));
	}

	DSPThread::~DSPThread()
	{
		join();
	}

	void DSPThread::join()
	{
		if(!m_thread)
			return;

		m_runThread = false;
		m_thread->join();
		m_thread.reset();
	}

	void DSPThread::threadFunc()
	{
#ifdef _WIN32
		::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
		size_t instructions = 0;
		size_t counter = 0;

		using Clock = std::chrono::high_resolution_clock;

		auto t = Clock::now();

#ifdef _DEBUG
		const size_t ipsStep = 0x0100000;
#else
		const size_t ipsStep = 0x1000000;
#endif
		while(m_runThread)
		{
			{
				Guard g(m_mutex);

				const auto iBegin = m_dsp.getInstructionCounter();

				for(size_t i=0; i<128; i += 8)
				{
					m_dsp.exec();
					m_dsp.exec();
					m_dsp.exec();
					m_dsp.exec();
					m_dsp.exec();
					m_dsp.exec();
					m_dsp.exec();
					m_dsp.exec();
				}

				instructions += m_dsp.getInstructionCounter() - iBegin;
				counter += 128;
			}

			if((counter & (ipsStep-1)) == 0)
			{
				const auto t2 = Clock::now();
				const auto d = t2 - t;

				const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);

				m_ips = instructions / ms.count();

				instructions = 0;
				t = t2;

				LOG("IPS: " << m_ips << "k");
				std::cout << "IPS: "<< m_ips << "k" << std::endl;
			}
		}
	}
}
