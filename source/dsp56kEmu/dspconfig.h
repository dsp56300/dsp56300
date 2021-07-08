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

#ifdef HAVE_X86_64
	constexpr bool g_jitSupported = true;
#else
	static_assert(false, "Not compiling for x64. there is no JIT engine support, emulator will be very slow! Remove this static_assert to confirm building anyway");
	constexpr bool g_jitSupported = false;
#endif
}
