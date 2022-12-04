#pragma once

#include "dsp56kEmu/dsp.h"

namespace dsp56k
{
	class Memory;
}

namespace dsp56kDebugger
{
	class Debugger;

	class DebuggerListener
	{
	public:
		explicit DebuggerListener(Debugger& _debugger);
		virtual ~DebuggerListener();

		Debugger& debugger() const
		{
			return m_debugger;
		}

		dsp56k::Memory& memory() const;

		virtual void evFocusAddress(dsp56k::TWord _addr) {}
		virtual void evFocusMemAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr) {}
		virtual void evToggleBreakpoint() {}
		virtual void evBreakpointsChanged() {}
		virtual void evMemBreakpointsChanged(dsp56k::EMemArea _area) {}
		virtual void evDspHalt(dsp56k::TWord _addr) {}
		virtual void evDspResume() {}
		virtual void evGotoAddress(dsp56k::TWord _addr) {}
		virtual void evGotoPC() {}
		virtual void evToggleBookmark() {}
		virtual void evGotoBookmark() {}

	private:
		Debugger& m_debugger;
	};
}
