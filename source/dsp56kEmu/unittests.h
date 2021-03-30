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
		void testMultiply();
		void testMultiply(int x0, int y0, int64_t expectedResult, TWord opcode);
		void testMoveImmediateToRegister();
		void testAdd();
		void testAdd(int64_t a, int y0, int64_t expectedResult);
		void testCCCC();
		void testCCCC(int64_t _val, int64_t _compareValue, bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq);
		void execOpcode(uint32_t _op0, uint32_t _op1 = 0, bool _reset=false);

		PeripheralsDefault peripherals;
		Memory mem;
		DSP dsp;
	};
}
