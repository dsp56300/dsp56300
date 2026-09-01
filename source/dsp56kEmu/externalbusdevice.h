#pragma once

#include "types.h"

namespace dsp56k
{
	/**
	 * A device on the external bus that is not plain memory. A flash chip for example needs to
	 * see a command sequence before a write takes effect, and answers reads with device IDs
	 * while it is in autoselect mode.
	 *
	 * Register one on the DSP and declare its address range via JitConfig::externalBusBegin and
	 * externalBusEnd. The JIT then routes accesses to that range here instead of to memory:
	 *
	 * - absolute addresses are classified while the block is compiled, so they cost nothing
	 * - dynamic addresses cost one unsigned range check, on writes only
	 *
	 * Dynamic reads are deliberately not routed. A device that keeps its contents in DSP memory
	 * answers them correctly all by itself, which is the case we care about. If a device ever
	 * needs to see dynamic reads as well, this is where that decision has to be revisited.
	 */
	class IExternalBusDevice
	{
	public:
		virtual ~IExternalBusDevice() = default;

		virtual TWord read(TWord _addr) = 0;
		virtual void write(TWord _addr, TWord _value) = 0;
	};
}
