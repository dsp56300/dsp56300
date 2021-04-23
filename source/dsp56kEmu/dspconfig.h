#pragma once

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
}
