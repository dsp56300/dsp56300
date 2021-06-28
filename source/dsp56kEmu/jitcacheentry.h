#pragma once

#include "types.h"

namespace dsp56k
{
	class JitBlock;
	class Jit;

	using TJitUpdateFunc = void (Jit::*)(TWord pc, JitBlock* _block);

	struct JitCacheEntry
	{
		TJitUpdateFunc func;
		JitBlock* block;
	};
}
