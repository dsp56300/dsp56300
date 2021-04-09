#pragma once
#include <cstdint>

#include "fastmath.h"
#include "utils.h"

namespace dsp56k
{	
	constexpr float g_float2dspScale	= 8388608.0f;
	constexpr float g_dsp2FloatScale	= 0.00000011920928955078125f;
	constexpr float g_dspFloatMax		= 8388607.0f;
	constexpr float g_dspFloatMin		= -8388608.0f;

	static TWord float2Dsdp(float f)
	{
		f *= g_float2dspScale;
		f = clamp(f, g_dspFloatMin, g_dspFloatMax);

		return floor_int(f) & 0x00ffffff;
	}

	static float dsp2Float(TWord d)
	{
		return static_cast<float>(signextend<int32_t,24>(d)) * g_dsp2FloatScale;
	}

	template<typename T> class Audio
	{
	};
}
