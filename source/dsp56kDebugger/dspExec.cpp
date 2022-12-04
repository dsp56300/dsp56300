#include "dspExec.h"

#include "debugger.h"
#include "mainWindow.h"
#include "dsp56kEmu/types.h"

namespace dsp56kDebugger
{
	DspExec::DspExec(Debugger& _debugger) : m_debugger(_debugger)
	{
	}

	void DspExec::onExec(dsp56k::TWord _addr)
	{
		if(m_execMode == ExecMode::Step || m_execMode == ExecMode::Break || m_debugger.getState().hasAddrBreakpoint(_addr))
		{
			haltDSP(_addr);
		}

		switch (m_execMode)
		{
		case ExecMode::StepOver:
			break;
		case ExecMode::StepOut:
			if(m_debugger.dsp().regs().sp.toWord() < m_stepOutStackPointer)
				haltDSP(_addr);
			break;
		}

		runToDspHookFunc();
	}

	void DspExec::onMemoryWrite(dsp56k::EMemArea _area, uint32_t _addr, uint32_t _value)
	{
		if(m_debugger.getState().hasMemBreakpoint(_area, _addr))
			haltDSP(m_debugger.dsp().getPC().toWord());
	}

	void DspExec::onDebug()
	{
		haltDSP(m_debugger.dsp().getPC().toWord());
	}

	void DspExec::execBreak()
	{
		m_execMode = ExecMode::Break;
	}

	void DspExec::execContinue()
	{
		resumeDsp(ExecMode::Run);
	}

	void DspExec::execStep()
	{
		resumeDsp(ExecMode::Step);
	}

	void DspExec::execStepOver()
	{
		const auto& r = m_debugger.dsp().regs();
		const auto pc = r.pc.toWord();

/*
		const auto branchTarget = m_debugger.getBranchTarget(pc, pc);

		if(branchTarget == dsp56k::g_invalidAddress || branchTarget == dsp56k::g_dynamicAddress)
		{
			execStep();
			return;
		}
*/
		const auto op = m_debugger.dsp().memory().get(dsp56k::MemArea_P, pc);
		const auto opLen = m_debugger.dsp().opcodes().getOpcodeLength(op);

		m_stepOverTargetAddr = pc + opLen;

		resumeDsp(ExecMode::StepOver);
	}

	void DspExec::execStepOut()
	{
		const auto sp = m_debugger.dsp().regs().sp.toWord();
		if(sp == 0)
			return;

		m_stepOutStackPointer = sp - 1;

		resumeDsp(ExecMode::StepOut);
	}

	void DspExec::haltDSP(const dsp56k::TWord _addr)
	{
		m_execMode = ExecMode::Break;

		runOnUiThread([&]()
		{
			m_debugger.sendEvent(&DebuggerListener::evDspHalt, _addr);
		});

		std::unique_lock uLock(m_haltDSPmutex);
		m_haltDSPcv.wait(uLock, [&]
		{
			runToDspHookFunc();
			return m_execMode != ExecMode::Break;
		});
	}

	void DspExec::resumeDsp(ExecMode _mode)
	{
		m_execMode = _mode;
		m_haltDSPcv.notify_one();
		m_debugger.sendEvent(&DebuggerListener::evDspResume);
	}

	void DspExec::runOnDspThread(const std::function<bool()>& _func)
	{
		while(m_pendingHookFunc)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

		m_toDspHookFunc = _func;
		m_pendingHookFunc = true;
		m_haltDSPcv.notify_one();
	}

	void DspExec::runOnUiThread(const std::function<void()>& _func)
	{
		wxTheApp->GetTopWindow()->GetEventHandler()->CallAfter(_func);
	}

	void DspExec::runToDspHookFunc()
	{
		if(!m_pendingHookFunc)
			return;

		if(m_toDspHookFunc())
		{
			m_toDspHookFunc = [](){ return true; };
			m_pendingHookFunc = false;
		}
	}
}
