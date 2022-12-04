#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>

#include "dsp56kEmu/types.h"

namespace dsp56kDebugger
{
	class Debugger;

	class DspExec
	{
	public:
		enum class ExecMode
		{
			Run,
			Step,
			StepOver,
			StepOut,
			Break
		};

		explicit DspExec(Debugger& _debugger);

		void onExec(dsp56k::TWord _addr);
		void onMemoryWrite(dsp56k::EMemArea _area, uint32_t _addr, uint32_t _value);
		void onDebug();

		void execBreak();
		void execContinue();
		void execStep();
		void execStepOver();
		void execStepOut();

		void runOnDspThread(const std::function<bool()>& _func);
		static void runOnUiThread(const std::function<void()>& _func);

		ExecMode getExecMode() const { return m_execMode; }
	private:
		void haltDSP(dsp56k::TWord _addr);
		void resumeDsp(ExecMode _mode);
		void runToDspHookFunc();

		Debugger& m_debugger;

		std::mutex m_haltDSPmutex;
		std::condition_variable m_haltDSPcv;

		ExecMode m_execMode = ExecMode::Run;
		dsp56k::TWord m_stepOutStackPointer = 0;
		dsp56k::TWord m_stepOverTargetAddr = 0;

		std::function<bool()> m_toDspHookFunc = [](){ return true; };
		bool m_pendingHookFunc = false;
	};
}
