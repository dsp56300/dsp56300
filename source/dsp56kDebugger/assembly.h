#pragma once

#include "debuggerListener.h"
#include "styledTextCtrl.h"

#include "wx/wx.h"
#include "wx/stc/stc.h"

#include "dsp56kEmu/disasm.h"

namespace dsp56kDebugger
{
	class Assembly : public StyledTextCtrl, public DebuggerListener
	{
	public:
		explicit Assembly(Debugger& _debugger, wxWindow* _parent);

		void onChange(wxStyledTextEvent& e);
		void onDoubleClick(wxStyledTextEvent& e);

	private:
		static std::string formatAsm(const dsp56k::Disassembler::Line& _line);
		void disassemble(int _line, int _count);
		void onTimer(wxTimerEvent&);
		void onCallTip(wxTimerEvent&);
		void onMotion(wxMouseEvent&);

		void evToggleBreakpoint() override;
		void evBreakpointsChanged() override;
		void evDspHalt(dsp56k::TWord _addr) override;
		void evGotoAddress(dsp56k::TWord _addr) override;
		void evGotoPC() override;
		void evToggleBookmark() override;
		void evGotoBookmark() override;

		void refresh();

		std::vector<bool> m_invalidCode;
		std::vector<uint32_t> m_addressToLine;
		std::vector<uint32_t> m_lineToAddress;

		wxTimer m_refreshTimer;
		wxTimer m_calltipTimer;

		wxDECLARE_EVENT_TABLE();
	};
}
