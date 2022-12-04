#pragma once

#include "debuggerListener.h"
#include "grid.h"

namespace dsp56kDebugger
{
	class Callstack final : public Grid, protected DebuggerListener
	{
	public:
		Callstack(Debugger& _debugger, wxWindow* _parent);

		void evDspHalt(dsp56k::TWord _addr) override;
	private:
		void refresh();
	};
}
