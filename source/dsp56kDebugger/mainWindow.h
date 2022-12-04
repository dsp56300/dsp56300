#pragma once

#include "wx/wx.h"
#include "wx/aui/framemanager.h"

namespace dsp56kDebugger
{
	class Debugger;

	class MainWindow : public wxFrame
	{
	public:
		explicit MainWindow(Debugger& _debugger);
		~MainWindow() override;

		void onTimer(wxTimerEvent& e);

		void onEditJitConfig(wxCommandEvent& e);

		void onToggleBreakpoint(wxCommandEvent& e);
		void onExecBreak(wxCommandEvent& e);
		void onExecStepInfo(wxCommandEvent& e);
		void onExecStepOver(wxCommandEvent& e);
		void onExecStepOut(wxCommandEvent& e);
		void onExecContinue(wxCommandEvent& e);

		void onGotoPC(wxCommandEvent& e);
		void onGotoAddress(wxCommandEvent& e);

		void onAddBookmark(wxCommandEvent& e);
		void onGotoBookmark(wxCommandEvent& e);

	private:
		Debugger& m_debugger;
		wxAuiManager m_aui;
		wxTimer m_timer;

		wxDECLARE_EVENT_TABLE();
	};
}
