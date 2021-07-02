#pragma once

#include <map>

#include "types.h"

namespace dsp56k
{
	class JitBlock;
	class Jit;

	typedef void (*TJitUpdateFunc)(Jit*, TWord, JitBlock*);

	struct JitCacheEntry
	{
		TJitUpdateFunc func;
		JitBlock* block;
		std::map<TWord,JitBlock*> singleOpCache;
	};
}
