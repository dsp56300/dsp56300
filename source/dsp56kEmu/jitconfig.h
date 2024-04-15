#pragma once

#include <cstdint>

namespace dsp56k
{
	struct JitConfig
	{
		bool aguSupportBitreverse = false;
		bool aguSupportMultipleWrapModulo = true;
		bool cacheSingleOpBlocks = true;
		bool linkJitBlocks = true;
		bool splitOpsByNops = false;
		bool dynamicPeripheralAddressing = false;
		uint32_t maxInstructionsPerBlock = 0;
		bool memoryWritesCallCpp = false;

		bool asmjitDiagnostics = false;
		uint32_t maxDoIterations = 0;	// maximum number of iterations of a do loop before the Jit block is exited (and later re-entered), giving a time slice for interrupts/peripherals
	};
}
