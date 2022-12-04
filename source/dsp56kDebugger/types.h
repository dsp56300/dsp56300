#pragma once

#include "wx/wx.h"

namespace dsp56kDebugger
{
	enum WindowIds
	{
		// Main Window Menu
		IdToggleBreakpoint = wxID_HIGHEST + 1,
		IdDebugBreak,
		IdDebugStepInto,
		IdDebugStepOver,
		IdDebugStepOut,
		IdDebugContinue,
		IdGotoPC,
		IdGotoBeginningOfJitBlock,
		IdGotoEndOfJitBlock,
		IdGotoNextJitBlock,
		IdGotoAddress,
		IdBookmarkAdd,
		IdBookmarkGoto,
		IdEditJitConfig,

		// Buttons
		IdButtonApply
	};
}
