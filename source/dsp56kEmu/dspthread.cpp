#include "dspthread.h"

#include <iostream>

#include "debuggerinterface.h"
#include "dsp.h"

#include "../vtuneSdk/include/ittnotify.h"

#if DSP56300_DEBUGGER
#include "dsp56kDebugger/debugger.h"
#endif

#ifdef WIN32
#include <Windows.h>
#endif

namespace dsp56k
{
#ifdef _WIN32
	constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	void SetThreadName( DWORD dwThreadID, const char* threadName)
	{
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = threadName;
		info.dwThreadID = dwThreadID;
		info.dwFlags = 0;

		__try  // NOLINT(clang-diagnostic-language-extension-token)
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info) );
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}

		__itt_thread_set_name(threadName);
	}
#endif

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

		m_runThread = false;

		m_dsp.terminate();

		m_thread->join();
		m_thread.reset();

		m_debugger.reset();
	}

	void DSPThread::setCurrentThreadName(const std::string& _name)
	{
#ifdef _WIN32
		SetThreadName(-1, _name.c_str());
#endif
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
#ifdef _WIN32
		::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
		setCurrentThreadName(m_name.empty() ? "DSP" : "DSP " + m_name);
#endif
		uint64_t instructions = 0;
		uint64_t counter = 0;

		uint64_t totalInstructions = 0;
		uint64_t totalCounter = 0;

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

				const auto d = delta(iEnd, iBegin);

				instructions += d;
				totalInstructions += d;

				counter += 128;

				m_callback(d);

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

				instructions = 0;
				t = t2;

				if(!m_name.empty())
					sprintf(m_mipsString, "[%s] MIPS: %.6f (%.6f average)", m_name.c_str(), m_currentMips, m_averageMips);
				else
					sprintf(m_mipsString, "MIPS: %.6f (%.6f average)", m_currentMips, m_averageMips);

				if(m_logToStdout)
					puts(m_mipsString);
				if(m_logToDebug)
					LOG(m_mipsString);
			}
		}

		m_dsp.setDebugger(m_nextDebugger);
	}
}
