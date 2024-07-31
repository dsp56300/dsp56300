#include "dspthread.h"

#include <iostream>

#include "debuggerinterface.h"
#include "dsp.h"
#include "threadtools.h"

#if DSP56300_DEBUGGER
#include "dsp56kDebugger/debugger.h"
#endif

namespace dsp56k
{
	void defaultCallback(uint32_t)
	{
	}

	DSPThread::DSPThread(DSP& _dsp, const char* _name/* = nullptr*/, std::shared_ptr<DebuggerInterface> _debugger/* = {}*/)
		: m_dsp(_dsp)
		, m_name(_name ? _name : std::string())
		, m_runThread(true)
		, m_debugger(std::move(_debugger))
	{
		if(m_debugger)
			setDebugger(m_debugger.get());

		setCallback(defaultCallback);

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

		if(m_debugger)
			detachDebugger(m_debugger.get());

		terminate();

		m_thread->join();
		m_thread.reset();

		m_debugger.reset();
	}

	void DSPThread::terminate()
	{
		m_runThread = false;

		m_dsp.terminate();
	}

	void DSPThread::setCallback(const Callback& _callback)
	{
		const Callback c = _callback ? _callback : defaultCallback;

		Guard g(m_mutex);
		m_callback = c;
	}

	void DSPThread::setDebugger(DebuggerInterface* _debugger)
	{
		std::lock_guard lock(m_debuggerMutex);
		if(m_nextDebugger)
			m_nextDebugger->setDspThread(nullptr);
		m_nextDebugger = _debugger;
		if(m_nextDebugger)
			m_nextDebugger->setDspThread(this);
	}

	void DSPThread::detachDebugger(const DebuggerInterface* _debugger)
	{
		std::lock_guard lock(m_debuggerMutex);
		if(m_nextDebugger == _debugger)
			setDebugger(nullptr);
	}

	void DSPThread::threadFunc()
	{
		ThreadTools::setCurrentThreadPriority(ThreadPriority::Highest);
		ThreadTools::setCurrentThreadName(m_name.empty() ? "DSP" : "DSP " + m_name);

		uint64_t instructions = 0;
		uint64_t cycles = 0;
		uint64_t counter = 0;

		uint64_t totalInstructions = 0;
		uint64_t totalCycles = 0;

		using Clock = std::chrono::high_resolution_clock;

		auto t = Clock::now();
		const auto tStart = t;

#ifdef _DEBUG
		constexpr size_t ipsStep = 0x0400000;
#else
		constexpr size_t ipsStep = 0x2000000;
#endif
		while(m_runThread)
		{
			{
				Guard g(m_mutex);

				const auto iBegin = m_dsp.getInstructionCounter();
				const auto cBegin = m_dsp.getCycles();

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

				const auto iEnd = m_dsp.getInstructionCounter();
				const auto cEnd = m_dsp.getCycles();

				const auto di = delta(iEnd, iBegin);
				const auto dc = delta(cEnd, cBegin);

				instructions += di;
				totalInstructions += di;

				cycles += dc;
				totalCycles += dc;

				counter += 128;

				m_callback(di);

#if DSP56300_DEBUGGER
				m_dsp.setDebugger(m_nextDebugger);
#endif
			}

			if((counter & (ipsStep-1)) == 0)
			{
				const auto t2 = Clock::now();
				const auto d = t2 - t;
				const auto dTotal = t2 - tStart;

				const auto ms = std::chrono::duration_cast<std::chrono::microseconds>(d);
				const auto msTotal = std::chrono::duration_cast<std::chrono::microseconds>(dTotal);

				m_currentMips = static_cast<double>(instructions) / static_cast<double>(ms.count());
				m_averageMips = static_cast<double>(totalInstructions) / static_cast<double>(msTotal.count());

				m_currentMcps = static_cast<double>(cycles) / static_cast<double>(ms.count());
				m_averageMcps = static_cast<double>(totalCycles) / static_cast<double>(msTotal.count());

				instructions = 0;
				cycles = 0;

				t = t2;

				if(!m_name.empty())
					snprintf(m_mipsString, std::size(m_mipsString), "[%s] MIPS: %.4f (%.4f avg), MHz: %.4f (%.4f avg)", m_name.c_str(), m_currentMips, m_averageMips, m_currentMcps, m_averageMcps);
				else
					snprintf(m_mipsString, std::size(m_mipsString), "MIPS: %.4f (%.4f avg), MHz: %.4f (%.4f avg)", m_currentMips, m_averageMips, m_currentMcps, m_averageMcps);

				if(m_logToStdout)
					puts(m_mipsString);
				if(m_logToDebug)
					LOG(m_mipsString);
			}
		}

		m_dsp.setDebugger(m_nextDebugger);
	}
}
