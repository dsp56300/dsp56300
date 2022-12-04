#include "statusBar.h"

#include "debugger.h"
#include "dsp56kEmu/dspthread.h"

namespace dsp56kDebugger
{
	std::string toString(DspExec::ExecMode _mode)
	{
		switch (_mode)
		{
		case DspExec::ExecMode::Run:		return "Running";
		case DspExec::ExecMode::Step:		return "Step";
		case DspExec::ExecMode::StepOver:	return "Step Over";
		case DspExec::ExecMode::StepOut:	return "Step Out";
		case DspExec::ExecMode::Break:		return "Break";
		default:							return "?";
		}
	}

	StatusBar::StatusBar(Debugger& _debugger, wxWindow* _parent) : wxStatusBar(_parent), DebuggerListener(_debugger)
	{
		SetFieldsCount(1);

		SetStatusText("one", 0);
//		SetStatusText("two", 1);
//		SetStatusText("three", 2);

		m_timer.Bind(wxEVT_TIMER, [&](wxTimerEvent&){refresh();});
		m_timer.Start(500, false);
	}

	void StatusBar::refresh()
	{
		const auto mode = debugger().dspExec().getExecMode();

		std::stringstream ss;
		ss << "DSP: " << toString(mode);

		if(mode == DspExec::ExecMode::Run)
		{
			const auto* thread = debugger().dspThread();

			if(thread)
			{
				const auto mips = thread->getCurrentMips();
				ss << ": " << mips << " Mips";
			}
		}
		else
		{
			ss << " @ " << HEX(debugger().getState().getCurrentPC());
		}

		SetStatusText(ss.str(), 0);
	}

	void StatusBar::evDspHalt(dsp56k::TWord _addr)
	{
		refresh();
	}

	void StatusBar::evDspResume()
	{
		refresh();
	}
}
