#pragma once

#include "buildconfig.h"

#if defined(HAVE_SSE)
#	include <xmmintrin.h>
#	include <emmintrin.h>
#endif

namespace dsp56k
{

// gcc does it right but not MSC
#if defined(_MSC_VER) && defined(HAVE_SSE)

inline int floor_int(float _value)
{
	return _mm_cvttss_si32(_mm_set_ss(_value));
}

inline int floor_int(double _value)
{
	return _mm_cvttsd_si32(_mm_set_sd(_value));
}

#	define		round_int(VAL)			floor_int((VAL)+0.5f)
#	define		ceil_int(VAL)			(0xffff - floor_int(0xffff-(VAL)))

#else
	inline int floor_int(float x)
	{
		return (int)x;
	}

	inline int round_int(float x)
	{
		return floor_int(x+0.5f);
	}

	inline int ceil_int(float x)
	{
		return 0xffff - floor_int(0xffff-x);
	}
#endif

	inline float fastexp3(float x) 
	{ 
		return (6+x*(6+x*(3+x)))*0.16666666f; 
	}

	inline float fastexp4(float x) 
	{
		return (24+x*(24+x*(12+x*(4+x))))*0.041666666f;
	}

	inline float fastexp5(float x) 
	{
		return (120+x*(120+x*(60+x*(20+x*(5+x)))))*0.0083333333f;
	}

	inline float fastexp6(float x) 
	{
		return 720+x*(720+x*(360+x*(120+x*(30+x*(6+x)))))*0.0013888888f;
	}

	inline float fastexp7(float x) 
	{
		return (5040+x*(5040+x*(2520+x*(840+x*(210+x*(42+x*(7+x)))))))*0.00019841269f;
	}

	inline float fastexp8(float x) 
	{
		return (40320+x*(40320+x*(20160+x*(6720+x*(1680+x*(336+x*(56+x*(8+x))))))))*2.4801587301e-5f;
	}

	inline float fastexp9(float x) 
	{
		return (362880+x*(362880+x*(181440+x*(60480+x*(15120+x*(3024+x*(504+x*(72+x*(9+x)))))))))*2.75573192e-6f;
	}

	static inline float	fastpow2 (float p)
	{
		float offset = (p < 0) ? 1.0f : 0.0f;
		float clipp = (p < -126) ? -126.0f : p;
		int w = floor_int(clipp);
		float z = clipp - w + offset;
		union { unsigned int i; float f; } v = { static_cast<unsigned int>( (1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z) ) };

		return v.f;
	}

	static inline float	fastpow2PositiveOnly (float clipp)
	{
		int w = floor_int(clipp);
		float z = clipp - w;
		union { unsigned int i; float f; } v = { static_cast<unsigned int>( (1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z) ) };

		return v.f;
	}
	static inline float fastexp (float p)
	{
		return fastpow2 (1.442695040f * p);
	}

	static inline float fastexpPositiveOnly (float p)
	{
		return fastpow2PositiveOnly (1.442695040f * p);
	}

#ifdef HAVE_SSE
	inline float min(float a, float b)
	{
		// Branchless SSE min.
		_mm_store_ss(&a, _mm_min_ss(_mm_set_ss(a), _mm_set_ss(b)));
		return a;
	}

	inline float max(float a, float b)
	{
		// Branchless SSE max.
		_mm_store_ss(&a, _mm_max_ss(_mm_set_ss(a), _mm_set_ss(b)));
		return a;
	}

	inline float clamp(float val, float minval, float maxval)
	{
		// Branchless SSE clamp.

		_mm_store_ss(&val, _mm_min_ss(_mm_max_ss(_mm_set_ss(val), _mm_set_ss(minval)), _mm_set_ss(maxval)));

		return val;
	}
#endif

	template<typename T> T min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}
	template<typename T> T max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template<typename T> T clamp(const T& _src, const T& _min, const T& _max)
	{
		const T t = _src < _min ? _min : _src;
		return t > _max ? _max : t;
	}

	template<typename T> T clamp01(const T& _src)
	{
		return clamp(_src, T(0), T(1));
	}

	template<typename T> T lerp(const T& a, const T& b, const T& val) { return a + (b - a) * val; }

	inline void blendControlForTwoParams(float& _dstA, float& _dstB, float _paramMinus1to1)
	{
		// _dstA will be:
		// ______
		//       \
		//        \
		//         \
		//          \

		// _dstB will be:
		//     ______
		//    /
		//   /
		//  /
		// /

		_dstA = min(1.f - _paramMinus1to1, 1.f);
		_dstB = min(_paramMinus1to1 + 1.f, 1.f);
	}
}
