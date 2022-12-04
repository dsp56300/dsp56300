#pragma once

#include "debuggerListener.h"

#include "wx/wx.h"

namespace dsp56kDebugger
{
	class StatusBar : public wxStatusBar, protected DebuggerListener
	{
	public:
		StatusBar(Debugger& _debugger, wxWindow* _parent);

	private:
		void refresh();

		void evDspHalt(dsp56k::TWord _addr) override;
		void evDspResume() override;

		wxTimer m_timer;
	};
}
