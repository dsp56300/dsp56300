#pragma once

#include "buildconfig.h"

namespace dsp56k
{
#ifdef DSP56K_56303
	using Peripheral = Peripherals56303;
#else
	using Peripheral = Peripherals56362;
#endif

#ifdef DSP56K_AAR_TRANSLATE
	constexpr bool g_useAARTranslate = true;
#else
	constexpr bool g_useAARTranslate = false;
#endif

#if defined(HAVE_X86_64) || defined(HAVE_ARM64)
	constexpr bool g_jitSupported = true;
#else
	constexpr bool g_jitSupported = false;
#endif
}
