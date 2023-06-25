#pragma once

#include "bitfield.h"
#include "types.h"

//#include "buildconfig.h"
//#include <intrin.h>

#ifdef _MSC_VER
#	define DSP56K_DEOPTIMIZE __pragma(optimize("", off))
#else
#	define DSP56K_DEOPTIMIZE
#endif

namespace dsp56k
{
	// TODO: optimize for x86 win32, use intrin.h that has those functions in it

	template<typename T, uint32_t B> Bit bitvalue(const T& _val)
	{
		return Bit((_val >> B) & 1);
	}

	template<typename T> Bit bitvalue(const T& _val, const TWord bit)
	{
		return Bit((_val >> bit) & 1);
	}

	template<uint32_t B, typename Type, uint32_t BitCount> Bit bitvalue(const RegType<Type,BitCount>& _val)
	{
		return bitvalue<typename RegType<Type,BitCount>::MyType,B>(_val.var);
	}

	template<uint32_t B> Bit bitvalue(const TReg56& _val)
	{
//#ifdef HAVE_SSE
//		return Bit(_bittest64(&_val.var, B));
//#endif
		return bitvalue<uint64_t,B>(_val.var);
	}

	template<typename T> bool bittest( const T& _val, unsigned int _bitNumber )
	{
		return (_val & (T(1)<<_bitNumber)) != 0;
	}

	template<typename T, T B> T bittest( const T& _val)
	{
		return _val & (T(1)<<B);
	}

	static int bittest( const TReg56& _val, unsigned int _bitNumber )
	{
		return bittest( _val.var, _bitNumber );
	}

	static int bittest( const TReg48& _val, unsigned int _bitNumber )
	{
		return bittest( _val.var, _bitNumber );
	}

	static int bittest( const TReg24& _val, unsigned int _bitNumber )
	{
		return bittest( _val.var, _bitNumber );
	}

	static int bittest( const TReg8& _val, unsigned int _bitNumber )
	{
		return bittest( _val.var, _bitNumber );
	}

	template<typename T> int bittestandset( T& _val, unsigned int _bitNumber )
	{
		auto res = bittest<T>( _val, _bitNumber );
		_val |= (T(1)<<_bitNumber);
		return res;
	}

	template<typename T, T B> void bitset( T& _val, const T& _zeroOrOne)
	{
		constexpr auto mask = static_cast<T>(1) << B;
		const auto bit = (_zeroOrOne&1) << B;

		_val &= ~mask;
		_val |= bit;
	}

	template<typename T> void bitset( T& _val, const T& _bit, const T& _zeroOrOne)
	{
		const auto mask = static_cast<T>(1) << _bit;
		const auto bit = _zeroOrOne << _bit;

		_val &= ~mask;
		_val |= bit;
	}

	template<typename T> void bitset( T& _val, const T& _bit, const Bit& _zeroOrOne)
	{
		bitset<T>(_val, _bit, _zeroOrOne.bit);
	}

	template<typename T,unsigned int B> int bittestandset( RegType<T,B>& _val, unsigned int _bitNumber )
	{
		return bittestandset( _val.var, _bitNumber );
	}

	template<typename T> int bittestandclear( T& _val, unsigned int _bitNumber )
	{
		auto res = bittest<T>( _val, _bitNumber );
		_val &= ~ (T(1)<<_bitNumber);
		return res;
	}

	template<typename T> auto bittestandchange( T& _val, unsigned int _bitNumber )
	{
		auto res = bittest<T>( _val, _bitNumber );

		_val ^= (T(1)<<_bitNumber);

		return res;
	}

	static int bittestandchange( TReg24& _val, unsigned int _bitNumber )
	{
		return bittestandchange( _val.var, _bitNumber );
	}

	template<typename T> unsigned int countZeroBitsReversed( const T& _val )
	{
		unsigned int count = 0;
		for( int bit = sizeof(T)*CHAR_BIT - 1; bit >= 0; --bit )
		{
			if( bittest(_val,bit) )
				return count;
			++count;
		}
		return count;
	}

	template<typename T,size_t numBitsSrc> T signextend(const T _src)
	{
		const T shiftAmount = (sizeof(T) * CHAR_BIT) - numBitsSrc;
		return (T(_src) << shiftAmount) >> shiftAmount;
	}

	static TUInt8 bitreverse8( TUInt8 a )
	{
		return	(a & 0x80 >> 7)
			|	(a & 0x40 >> 5)
			|	(a & 0x20 >> 3)
			|	(a & 0x10 >> 1)
			|	(a & 0x08 << 1)
			|	(a & 0x04 << 3)
			|	(a & 0x02 << 5)
			|	(a & 0x01 << 7);
	}

	static constexpr TWord bitreverse24( TWord x )
	{
		x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
		x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
		x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));

		return ((x & 0xff0000) >> 16) | (x & 0x00ff00) | ((x & 0x0000ff) << 16);
	}

	static TWord delta(TWord b, TWord a)
	{
		auto d = b - a;

		if (d & 0x80000000) 
			d = (d^0xFFFFFFFF)+1;

		return d;
	}

	constexpr size_t g_cacheLineSize	= 128;

	constexpr size_t alignedSize(size_t _size)
	{
		return (_size + g_cacheLineSize - 1) / g_cacheLineSize * g_cacheLineSize;
	}

	template<typename T>
	T* alignedAddress(T* _ptr)
	{
		static_assert(sizeof(size_t) == sizeof(T*), "pointer size invalid");
		const auto addr = reinterpret_cast<size_t>(_ptr);
		return reinterpret_cast<T*>((addr + g_cacheLineSize - 1) / g_cacheLineSize * g_cacheLineSize);
	}

	template<typename T> constexpr size_t alignedSize()
	{
		return alignedSize(sizeof(T));
	}

	void nativeDebugBreak();
}
