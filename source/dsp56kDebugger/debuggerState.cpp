#include "debuggerState.h"
#include "debugger.h"

namespace dsp56kDebugger
{
	State::State(Debugger& _debugger) : DebuggerListener(_debugger), m_focusedMemAddr({0}), m_memoryBreakpoints({})
	{
	}

	void State::setFocusedAddr(dsp56k::TWord _addr)
	{
		if(_addr == m_focusedAddr)
			return;
		debugger().sendEvent(&DebuggerListener::evFocusAddress, _addr);
	}

	void State::setFocusedMemoryAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr)
	{
		if(m_focusedMemAddr[_area] == _addr)
			return;
		debugger().sendEvent(&DebuggerListener::evFocusMemAddress, _area, _addr);
	}

	void State::toggleAddrBreakpoint()
	{
		toggleAddrBreakpoint(m_focusedAddr);
	}

	void State::toggleAddrBreakpoint(const dsp56k::TWord _addr)
	{
		const auto it = m_breakpoints.find(_addr);
		if(it != m_breakpoints.end())
			m_breakpoints.erase(it);
		else
			m_breakpoints.insert(_addr);

		debugger().sendEvent(&DebuggerListener::evBreakpointsChanged);
	}

	bool State::hasAddrBreakpoint(const dsp56k::TWord _addr) const
	{
		return m_breakpoints.find(_addr) != m_breakpoints.end();
	}

	void State::toggleMemBreakpoint(dsp56k::EMemArea _area)
	{
		toggleMemBreakpoint(_area, m_focusedMemAddr[_area]);
	}

	void State::toggleMemBreakpoint(const dsp56k::EMemArea _area, dsp56k::TWord _addr)
	{
		const auto it = m_memoryBreakpoints[_area].find(_addr);
		if(it != m_memoryBreakpoints[_area].end())
			m_memoryBreakpoints[_area].erase(it);
		else
			m_memoryBreakpoints[_area].insert(_addr);

		debugger().sendEvent(&DebuggerListener::evMemBreakpointsChanged, _area);
	}

	bool State::hasMemBreakpoint(dsp56k::EMemArea _area, dsp56k::TWord _addr) const
	{
		return m_memoryBreakpoints[_area].find(_addr) != m_memoryBreakpoints[_area].end();
	}

	void State::evFocusAddress(dsp56k::TWord _addr)
	{
		m_focusedAddr = _addr;
	}

	void State::evFocusMemAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr)
	{
		m_focusedMemAddr[_area] = _addr;
	}
}
