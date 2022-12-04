#pragma once

#include "debuggerListener.h"

namespace dsp56kDebugger
{
	class State : DebuggerListener
	{
	public:
		State(Debugger& _debugger);

		void setFocusedAddr(dsp56k::TWord _addr);
		dsp56k::TWord getFocusedAddress() const { return m_focusedAddr; }

		void setFocusedMemoryAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr);
		dsp56k::TWord getFocusedMemoryAddress(dsp56k::EMemArea _area) const { return m_focusedMemAddr[_area]; }

		void toggleAddrBreakpoint();
		void toggleAddrBreakpoint(dsp56k::TWord _addr);
		bool hasAddrBreakpoint(dsp56k::TWord _addr) const;

		void toggleMemBreakpoint(dsp56k::EMemArea _area);
		void toggleMemBreakpoint(dsp56k::EMemArea _area, dsp56k::TWord _addr);
		bool hasMemBreakpoint(dsp56k::EMemArea _area, dsp56k::TWord _addr) const;

		void setCurrentPC(const dsp56k::TWord _addr)
		{
			m_currentPC = _addr;
		}

		dsp56k::TWord getCurrentPC() const
		{
			return m_currentPC;
		}

	private:
		void evFocusAddress(dsp56k::TWord _addr) override;
		void evFocusMemAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr) override;

		dsp56k::TWord m_focusedAddr = 0;
		std::set<dsp56k::TWord> m_breakpoints;

		std::array<dsp56k::TWord, dsp56k::MemArea_COUNT> m_focusedMemAddr;
		std::array<std::set<dsp56k::TWord>, dsp56k::MemArea_COUNT> m_memoryBreakpoints;

		dsp56k::TWord m_currentPC = 0;
	};
}
