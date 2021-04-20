#pragma once

#include <cstdint>
#include <limits>
#include "assert.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

namespace dsp56k
{
	typedef uint8_t				TUInt8;
	typedef uint16_t			TUInt16;
	typedef uint32_t			TUInt32;
	typedef int32_t				TInt32;
	typedef uint64_t			TUInt64;
	typedef int64_t				TInt64;

	typedef uint32_t			TWord;
	typedef uint32_t			TDWord;

	template<typename T,unsigned int B> struct RegType
	{
		static constexpr T bitCount = B;
		static constexpr T bitMask = ((T(1)<<bitCount)-1);

		typedef T		MyType;

		T var;

		RegType() : var(0)													{}
		RegType( const RegType<T,B>& _other ) : var(_other.var)				{}
		RegType( RegType<T,B>&& _other ) noexcept : var(_other.var)			{}
		template<typename TYPE> explicit RegType( const TYPE& _var )		{ convert(*this,_var); }
//		template<typename TYPE> explicit RegType( const TYPE _var )			{ convert(*this,_var); }

		~RegType() = default;
		
		bool operator ==			( const RegType<T,B>& _ref ) const		{ return var == _ref.var; }
		bool operator ==			( const T& _ref ) const					{ return var == _ref; }

		bool operator !=			( const T& _ref ) const					{ return var != _ref; }
		bool operator !=			( const RegType& _ref ) const			{ return var != _ref.var; }

		T operator >>				( const T& _shift ) const				{ return var >> _shift;	}
		T operator <<				( const T& _shift ) const				{ return var << _shift;	}
// 		T operator &				( const T& _other ) const				{ return var & _other;	}
// 		T operator |				( const T& _other ) const				{ return var | _other;	}

//		void operator &=			( const T& _ref )						{ var &= _ref; }

//		RegType<T,B>	operator - () const									{ return RegType<T,B>(-var); }

		RegType& operator = (const RegType& other)
		{
			var = other.var;
			return *this;
		}

		RegType& operator = (RegType&& other) noexcept
		{
			var = other.var;
			return *this;
		}

		template<typename D> D signextend() const
		{
			const D shiftAmount = (sizeof(D) * CHAR_BIT) - bitCount;
			static_assert( shiftAmount > 0, "Invalid destination data size" );
			return (D(var) << shiftAmount) >> shiftAmount;
		}

		TWord toWord() const
		{
			return static_cast<TWord>(var);
		}

		void doMasking()		{ var &= bitMask; }
	};

	typedef RegType<uint8_t,5>		TReg5;
	typedef RegType<uint8_t,8>		TReg8;
	typedef RegType<int32_t,24>		TReg24;
	typedef RegType<int64_t,48>		TReg48;
	typedef RegType<int64_t,56>		TReg56;

	typedef RegType<TWord,24>		TMem;

	// required for stack
// 	static void operator |=	( TReg24& _val, const TWord _mask )	{ _val.var |= _mask; }
// 	static void operator &=	( TReg24& _val, const TWord _mask )	{ _val.var &= _mask; }

	// 48 bit conversion (X/Y)
	static TReg24 loword( const TReg48& _src ) 					{ return TReg24(static_cast<int32_t>(_src.var & 0xffffff)); }
	static TReg24 hiword( const TReg48& _src )					{ return TReg24(static_cast<int32_t>((_src.var & 0xffffff000000) >> 24)); }

	static void loword( TReg48& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0xffffff000000) | _src.var; }
	static void hiword( TReg48& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0x000000ffffff) | (static_cast<TReg48::MyType>(_src.var & 0xffffff)<<24); }

	// 56 bit conversion (accumulators A/B)
	static TReg24 loword( const TReg56& _src ) 					{ return TReg24(static_cast<int32_t>(_src.var & 0xffffff)); }
	static TReg24 hiword( const TReg56& _src )					{ return TReg24(static_cast<int32_t>((_src.var & 0xffffff000000) >> 24)); }

	static void loword( TReg56& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0xffffffff000000) | _src.var; }
	static void hiword( TReg56& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0xff000000ffffff) | (static_cast<TReg56::MyType>(_src.var & 0xffffff)<<24); }

	static TReg8 extword( const TReg56& _src )					{ return TReg8(static_cast<uint8_t>((_src.var >> 48) & 0xff)); }
	static void  extword( TReg56& _dst, const TReg8& _src )		{ _dst.var = (_dst.var & 0x00ffffffffffff) | (static_cast<TReg56::MyType>(_src.var & 0xff)<<48); }

	// SR/OMR conversion
	static TReg8 byte0( const TReg24& _src )					{ return TReg8( static_cast<uint8_t>(_src.var & 0xff) ); }
	static TReg8 byte1( const TReg24& _src )					{ return TReg8( static_cast<uint8_t>((_src.var & 0xff00) >> 8) ); }
	static TReg8 byte2( const TReg24& _src )					{ return TReg8( static_cast<uint8_t>((_src.var & 0xff0000) >> 16) ); }

	static void byte0( TReg24& _dst, const TReg8& _src )		{ _dst.var = (_dst.var & 0xffff00) | static_cast<int32_t>(_src.var); }
	static void byte1( TReg24& _dst, const TReg8& _src )		{ _dst.var = (_dst.var & 0xff00ff) | (static_cast<int32_t>(_src.var)<<8); }
	static void byte2( TReg24& _dst, const TReg8& _src )		{ _dst.var = (_dst.var & 0x00ffff) | (static_cast<int32_t>(_src.var)<<16); }

	// -----------------------

	// Move to accumulator
	static void convert( TReg56& _dst, const TReg8& _src )			{ _dst.var = _src.signextend<TReg56::MyType>()<<40; _dst.doMasking(); }
	static void convert( TReg56& _dst, const TReg24& _src )			{ _dst.var = _src.signextend<TReg56::MyType>()<<24; _dst.doMasking(); }
	static void convert( TReg56& _dst, const TReg48& _src )			{ _dst.var = _src.signextend<TReg56::MyType>(); _dst.doMasking(); }
	static void convert( TReg56& _dst, const TWord& _src )			{ _dst.var = static_cast<TReg56::MyType>(_src)<<24; }
	static void convert( TReg56& _dst, const TInt32& _src )			{ _dst.var = static_cast<TReg56::MyType>(_src)<<24; }
	static void convert( TReg56& _dst, const TInt64& _src )			{ _dst.var = _src & TReg56::bitMask; }
	static void convert( TReg56& _dst, const TUInt64& _src )		{ _dst.var = _src & TReg56::bitMask; }

	// Move to 24-bit register
	static void convert( TReg24& _dst, const TReg24& _src )			{ _dst.var = _src.var; }
	static void convert( TReg24& _dst, const TWord _src )			{ _dst.var = _src; }

	static void convertS( TWord& _dst, const TReg8& _src )			{ _dst = _src.signextend<int32_t>()&0x00ffffff; }
	static void convertS( TReg24& _dst, const TReg8& _src )			{ _dst.var = _src.signextend<int32_t>()&0x00ffffff; }
	static void convertS( TReg24& _dst, const TReg24& _src)			{ _dst.var = _src.var; }

	static void convertU( TReg8& _dst, const TReg8& _src )			{ _dst.var = _src.var; }
	static void convertU( TReg8& _dst, const TReg24& _src )			{ _dst.var = static_cast<TReg8::MyType>(_src.var & 0xff); }
	static void convertU( TReg24& _dst, const TReg8& _src )			{ _dst.var = _src.var; }
	static void convertU( TReg24& _dst, const TReg24& _src)			{ _dst.var = _src.var; }

	// Move to accumulator extension register
	static void convert( TReg8& _dst, const TReg8& _src )			{ _dst.var = _src.var; }
	static void convert( TReg8& _dst, const TReg24& _src )			{ _dst.var = static_cast<TReg8::MyType>(_src.var & TReg8::bitMask); }
	static void convert( TReg8& _dst, const TReg8::MyType& _src )	{ _dst.var = _src & TReg8::bitMask; }

	// Move to memory
	static void convert( TWord& _dst, const TReg24& _src )			{ _dst = _src.var; }
	static void convert( TWord& _dst, const TReg8& _src )			{ _dst = _src.var; }

	static void convert( TReg5& _dst, TReg5::MyType _src )			{ _dst.var = _src & TReg5::bitMask; }


//	TReg56 operator & (const TReg56& a, const TReg56& b )	{ return TReg56(a.var & b.var); }
	enum EMemArea
	{
		MemArea_X,
		MemArea_Y,
		MemArea_P,

		MemArea_COUNT,
	};

	extern const char g_memAreaNames[MemArea_COUNT];
}
