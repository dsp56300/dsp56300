#pragma once

#include "debuggerListener.h"
#include "styledTextCtrl.h"
#include "dsp56kEmu/types.h"
#include "wx/timer.h"

namespace dsp56kDebugger
{
	class Memory : public StyledTextCtrl, protected DebuggerListener
	{
	public:
		explicit Memory(Debugger& _debugger, wxWindow* _parent, dsp56k::EMemArea _area);

	private:
		void initialize();
		void refresh();
		void onTimer(wxTimerEvent& e);
		void selectAddress(dsp56k::TWord _addr);
		void update(int _line, int _count);
		dsp56k::TWord addressFromPos(int _pos) const;
		void onLeftMouseDown(wxMouseEvent& e);
		void addressToPositions(int& _start, int& _end, dsp56k::TWord _addr) const;

		void evToggleBreakpoint() override;
		void evGotoAddress(dsp56k::TWord _addr) override;
		void evToggleBookmark() override;
		void evGotoBookmark() override;
		void evFocusMemAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr) override;
		void evMemBreakpointsChanged(dsp56k::EMemArea _area) override;

		wxTimer m_timer;
		uint32_t m_columnCount = 8;
		dsp56k::EMemArea m_area;
		dsp56k::TWord m_selectedAddr = 0;

		wxDECLARE_EVENT_TABLE();
	};
}
