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
}
