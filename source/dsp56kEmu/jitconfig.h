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
	};
}
