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

		// Assert a word is refused by the decoder (rendered as dc), and that neither
		// Add_SD nor Sub_SD claims it.
		void expectReserved(uint32_t _opA, uint32_t _opB = 0);

		// Assert a word still decodes to a non-dc instruction.
		void expectDecodes(uint32_t _opA, uint32_t _opB = 0);

		// Test individual instruction categories
		void testAluInstructions();
		void testMoveInstructions();
		void testBitInstructions();
		void testBranchInstructions();
		void testLoopInstructions();
		void testMiscInstructions();
		void testParallelInstructions();
		void testPeripheralSymbols();
		void testReservedAluEncodings();

		uint32_t m_testCount = 0;
		uint32_t m_passCount = 0;
		uint32_t m_failCount = 0;
	};
}
