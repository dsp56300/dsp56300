#include "debuggerListener.h"
#include "debugger.h"

namespace dsp56kDebugger
{
	DebuggerListener::DebuggerListener(Debugger& _debugger): m_debugger(_debugger)
	{
		_debugger.addListener(this);
	}

	DebuggerListener::~DebuggerListener()
	{
		m_debugger.removeListener(this);
	}

	dsp56k::Memory& DebuggerListener::memory() const
	{
		return m_debugger.dsp().memory();
	}
}
