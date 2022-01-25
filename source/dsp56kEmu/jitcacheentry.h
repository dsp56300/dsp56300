#pragma once

#include <map>

#include "types.h"

namespace dsp56k
{
	class JitBlock;
	class Jit;

	typedef void (*TJitFunc)(Jit*, TWord);

	struct JitCacheEntry
	{
		JitBlock* block = nullptr;
		std::map<TWord,JitBlock*> singleOpCache;
	};
}
