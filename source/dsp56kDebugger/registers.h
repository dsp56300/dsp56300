#pragma once

#include "debuggerListener.h"

#include "wx/timer.h"

#include "grid.h"

namespace dsp56kDebugger
{
	class Debugger;

	class Registers : public Grid, protected DebuggerListener
	{
	public:
		void initColor(const wxPoint& _pos, const wxColour& _col);
		Registers(Debugger& _debugger, wxWindow* _parent);

	private:
		template<typename T,unsigned int B> void updateCellT(const dsp56k::RegType<T,B>& _value, const wxPoint& _pos)
		{
			updateCell(_value.var, B, _pos);
		}
		void updateCell(uint64_t _value, uint32_t _bits, const wxPoint& _pos);
		void refresh();
		void onTimer(wxTimerEvent& e);
		void initCell(const std::string& _label, const wxPoint& _pos);

		void evDspHalt(dsp56k::TWord _addr) override
		{
			refresh();
		}

		wxTimer m_timer;
	};
}
