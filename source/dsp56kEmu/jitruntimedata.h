#pragma once

#include "types.h"

namespace dsp56k
{
	constexpr TWord g_pcInvalid = 0xffffffff;

	struct JitRuntimeData
	{
		TWord m_pMemWriteAddress = g_pcInvalid;
		TWord m_pMemWriteValue = 0;
	};
}
