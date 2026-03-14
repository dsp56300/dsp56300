#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dsp56k
{
	class AssemblerTest
	{
	public:
		AssemblerTest();

	private:
		// Round-trip test: disassemble hex → assemble text → verify hex matches
		void roundTrip(uint32_t _opA, uint32_t _opB = 0);

		// Test individual instruction categories
		void testAluInstructions();
		void testMoveInstructions();
		void testBitInstructions();
		void testBranchInstructions();
		void testLoopInstructions();
		void testMiscInstructions();
		void testParallelInstructions();
		void testPeripheralSymbols();

		uint32_t m_testCount = 0;
		uint32_t m_passCount = 0;
		uint32_t m_failCount = 0;
	};
}
