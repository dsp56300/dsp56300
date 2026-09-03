#pragma once

#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <thread>

#ifndef _WIN32
#include <pthread.h>
#endif

#include "debuggerinterface.h"
#include "dsp56kBase/threadtools.h"

namespace dsp56k
{
	class DSP;

	class DSPThread final
	{
	public:
		using Guard = std::lock_guard<std::mutex>;
		using Callback = std::function<void(uint32_t)>;

		// _initialPriority is the priority the worker runs at from thread start. Defaults to Highest (the
		// realtime band) for steady-state audio; nova passes a lower boot priority and raises to Highest only
		// once booted, because on macOS a thread that starts Highest cannot later drop off the RT band.
		explicit DSPThread(DSP& _dsp, const char* _name = nullptr, std::shared_ptr<DebuggerInterface> _debugger = {}, ThreadPriority _initialPriority = ThreadPriority::Highest);
		~DSPThread();
		void join();
		void terminate();

		std::mutex& mutex() { return m_mutex; }

		void setCallback(const Callback& _callback);

		void setLogToDebug(const bool _log) { m_logToDebug = _log; }
		void setLogToStdout(const bool _log) { m_logToStdout = _log; }

		const char* getMipsString() const { return m_mipsString; }
		double getCurrentMips() const { return m_currentMips; }
		double getAverageMips() const { return m_averageMips; }

		void setDebugger(DebuggerInterface* _debugger);
		void detachDebugger(const DebuggerInterface* _debugger);

		DSP& dsp() { return m_dsp; }

		bool runThread() const { return m_runThread; }

	private:
		void threadFunc();

		DSP& m_dsp;
		const std::string m_name;
		const ThreadPriority m_initialPriority;

		std::mutex m_mutex;
#ifdef _WIN32
		std::unique_ptr<std::thread> m_thread;
#else
		// pthread instead of std::thread so the stack size can be set, see the constructor
		pthread_t m_thread = {};
		bool m_threadStarted = false;
#endif

		bool m_runThread;

		Callback m_callback;

		std::recursive_mutex m_debuggerMutex;
		DebuggerInterface* m_nextDebugger = nullptr;

		std::shared_ptr<DebuggerInterface> m_debugger;

		double m_currentMips = 0.0;
		double m_averageMips = 0.0;

		double m_currentMcps = 0.0;
		double m_averageMcps = 0.0;

		char m_mipsString[128]{0};

		bool m_logToDebug = true;
		bool m_logToStdout = false;
	};
}
