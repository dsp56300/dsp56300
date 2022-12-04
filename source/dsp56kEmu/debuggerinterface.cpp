#include "debuggerinterface.h"

#include "dsp.h"

namespace dsp56k
{
	DebuggerInterface::DebuggerInterface(DSP& _dsp) : m_dsp(_dsp)
	{
	}

	DebuggerInterface::~DebuggerInterface()
	{
		assert(!isAttached());
	}

	void DebuggerInterface::onAttach()
	{
		assert(!isAttached());
		m_attached = true;
	}

	void DebuggerInterface::onDetach()
	{
		assert(isAttached());
		m_attached = false;
	}
}
