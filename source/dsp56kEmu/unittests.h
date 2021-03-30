#pragma once

#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	class UnitTests
	{
	public:
		void testMultiply();
		void testMoveImmediateToRegister();
		UnitTests();
	private:
		void testCCCC();
		void testCCCC(int64_t _val, int64_t _compareValue, bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq);
		void execOpcode(uint32_t _op0, uint32_t _op1 = 0, bool _reset=false);

		Memory mem;
		DSP dsp;
	};
}
