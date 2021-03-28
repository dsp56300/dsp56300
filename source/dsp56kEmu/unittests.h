#pragma once

#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	class UnitTests
	{
	public:
		UnitTests();
	private:
		void testCCCC(int64_t _val, int64_t _compareValue, bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq);

		Memory mem;
		DSP dsp;
	};
}
