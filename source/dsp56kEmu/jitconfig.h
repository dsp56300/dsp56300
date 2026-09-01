#pragma once

#include <cstdint>
#include <optional>
#include <functional>

#include "types.h"

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

		// 16 bit compatibility mode for AGU operations are not supported by default, set to true if needed
		bool support16BitSCMode = false;

		/*	A DO loop whose last instruction is a BSR/JSR. Hardware detects the loop end at fetch, so
			the call pushes the already-updated next PC and returns back INTO the loop.

			This is DOCUMENTED UNDEFINED OPERATION: DSP56300 Family Manual rev. 5, appendix A.3.1
			"Restrictions Near the End of DO Loops" forbids JMP/JSR/BRA/BSR (and the conditional
			forms) both at LA and at LA-1, and the assembler flags them as errors. Real firmware
			does it anyway and real silicon runs it, so the behaviour emulated here was taken from
			the Freescale reference simulator rather than from the manual: while LC > 1 the call
			pushes the LOOP START and LC is decremented; on the final iteration the loop retires
			FIRST (LA/LC popped, LF restored) and only then does the call push the address after
			the loop. A one-word call at LA and a two-word call starting at LA-1 behave alike.

			Off by default - only enable it for a device whose firmware needs it. A CONDITIONAL
			branch at a loop end is still not covered and asserts in debug builds.
		*/
		bool supportBranchAtLoopEnd = false;

		// maximum number of iterations of a do loop before the Jit block is exited (and later re-entered), giving a time slice for interrupts/peripherals
		uint32_t maxDoIterations = 0;

		// needs to be true if there is code that executes code in interrupt regions as regular jumps
		bool dynamicFastInterrupts = false;

		// asmjit can validate the generate code, usually not needed
		bool asmjitDiagnostics = false;

		// enable JIT optimizer (dead code elimination + constant folding)
		bool enableOptimizer = true;

		// x86-64 only: Will issue int3() = breakpoint interrupt if a memory address is detected that points to peripherals but DPA is disabled
		bool debugDynamicPeripheralAddressing = false;

		// Address range on the external bus that is served by DSP::getExternalBusDevice() instead
		// of by memory, see externalbusdevice.h. Empty by default, which keeps the generated code
		// bit identical to a build without an external bus device.
		TWord externalBusBegin = 0;
		TWord externalBusEnd = 0;		// exclusive

		// retrieves a JitConfig for a specific PC. If null, the global default config is used
		std::function<std::optional<JitConfig>(TWord)> getBlockConfig;
	};
}
