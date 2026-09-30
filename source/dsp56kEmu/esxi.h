// Base class for both ESAI (DSP 56362) and ESSI (DSP 56303)

#pragma once

#include "audio.h"

namespace dsp56k
{
	class Esxi : public Audio
	{
	public:
		virtual void execTX() = 0;
		virtual void execRX() = 0;

		virtual TWord hasEnabledTransmitters() const = 0;
		virtual TWord hasEnabledReceivers() const = 0;

		// an interrupt request of this interface is pending that the DSP has not taken yet
		virtual bool hasPendingInterrupts() const = 0;
	};
}
