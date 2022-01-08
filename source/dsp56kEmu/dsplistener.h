#pragma once

#include "types.h"

namespace dsp56k
{
	class DSPListener
	{
	public:
		virtual void onPmemWrite(const dsp56k::TWord _addr) = 0;
	};
}