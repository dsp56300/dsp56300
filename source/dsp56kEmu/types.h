#pragma once

namespace dsp56k
{
	typedef unsigned char		TUInt8;
	typedef unsigned short		TUInt16;
	typedef unsigned int		TUInt32;
	typedef int					TInt32;
	typedef unsigned __int64	TUInt64;
	typedef __int64				TInt64;

	typedef unsigned int		TWord;
	typedef unsigned __int64	TDWord;

	template<typename T,unsigned int B> struct RegType
	{
		static const T bitCount;
		static const T bitMask;

		typedef T		MyType;

		T var;

		RegType() : var(0)													{}
		RegType( const RegType<T,B>& _other ) : var(_other.var)				{}
		template<typename TYPE> explicit RegType( const TYPE& _var )		{ convert(*this,_var); }

		bool operator ==			( const RegType<T,B>& _ref ) const		{ return var == _ref.var; }
		bool operator ==			( const T& _ref ) const					{ return var == _ref; }

		bool operator !=			( const T& _ref ) const					{ return var != _ref; }
		bool operator !=			( const RegType& _ref ) const			{ return var != _ref.var; }

		T operator >>				( const T& _shift ) const				{ return var >> _shift;	}
		T operator <<				( const T& _shift ) const				{ return var << _shift;	}
// 		T operator &				( const T& _other ) const				{ return var & _other;	}
// 		T operator |				( const T& _other ) const				{ return var | _other;	}

//		void operator &=			( const T& _ref )						{ var &= _ref; }

		RegType<T,B>	operator - () const									{ return RegType<T,B>(-var); }

		template<typename D> D signextend() const
		{
			const D shiftAmount = (sizeof(D) * CHAR_BIT) - bitCount;
			assert( shiftAmount > 0 );
			return (D(var) << shiftAmount) >> shiftAmount;
		}

		TWord toWord() const
		{
			return TWord(var);
		}

		void doMasking()		{ var &= bitMask; }
	};

	typedef RegType<unsigned char,5>			TReg5;
	typedef RegType<unsigned char,8>			TReg8;
	typedef RegType<signed int,24>				TReg24;
	typedef RegType<signed __int64,48>			TReg48;
	typedef RegType<signed __int64,56>			TReg56;

	// required for stack
// 	static void operator |=	( TReg24& _val, const TWord _mask )	{ _val.var |= _mask; }
// 	static void operator &=	( TReg24& _val, const TWord _mask )	{ _val.var &= _mask; }

	// 48 bit conversion (X/Y)
	static TReg24 loword( const TReg48& _src ) 					{ return TReg24(int(_src.var & 0xffffff)); }
	static TReg24 hiword( const TReg48& _src )					{ return TReg24(int((_src.var & 0xffffff000000) >> 24)); }

	static void loword( TReg48& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0xffffff000000) | _src.var; }
	static void hiword( TReg48& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0x000000ffffff) | (TReg48::MyType(_src.var&0xffffff)<<24); }

	// 56 bit conversion (accumulators A/B)
	static TReg24 loword( const TReg56& _src ) 					{ return TReg24(int(_src.var & 0xffffff)); }
	static TReg24 hiword( const TReg56& _src )					{ return TReg24(int((_src.var & 0xffffff000000) >> 24)); }

	static void loword( TReg56& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0xffffffff000000) | _src.var; }
	static void hiword( TReg56& _dst, const TReg24& _src )		{ _dst.var = (_dst.var & 0xff000000ffffff) | (TReg56::MyType(_src.var&0xffffff)<<24); }

	static TReg8 extword( const TReg56& _src )					{ return TReg8(char((_src.var>>48) & 0xff)); }
	static void  extword( TReg56& _dst, const TReg8& _src )		{ _dst.var = (_dst.var & 0x00ffffffffffff) | (TReg56::MyType(_src.var&0xff)<<48); }

	// SR/OMR conversion
	static TReg8 byte0( const TReg24& _src )					{ return TReg8( (char)(_src.var & 0xff) ); }
	static TReg8 byte1( const TReg24& _src )					{ return TReg8( (char)((_src.var & 0xff00)>>8) ); }
	static TReg8 byte2( const TReg24& _src )					{ return TReg8( (char)((_src.var & 0xff0000)>>16) ); }

	static void byte0( TReg24& _dst, TReg8 _src )				{ _dst.var = (_dst.var & 0xffff00) | int(_src.var); }
	static void byte1( TReg24& _dst, TReg8 _src )				{ _dst.var = (_dst.var & 0xff00ff) | (int(_src.var)<<8); }
	static void byte2( TReg24& _dst, TReg8 _src )				{ _dst.var = (_dst.var & 0x00ffff) | (int(_src.var)<<16); }

	// -----------------------

	// Move to accumulator
	static void convert( TReg56& _dst, TReg8 _src )				{ _dst.var = _src.signextend<TReg56::MyType>()<<40; }
	static void convert( TReg56& _dst, TReg24 _src )			{ _dst.var = _src.signextend<TReg56::MyType>()<<24; }
	static void convert( TReg56& _dst, const TReg48& _src )		{ _dst.var = _src.signextend<TReg56::MyType>(); }
	static void convert( TReg56& _dst, const TWord& _src )		{ _dst.var = TReg56::MyType(_src)<<24; }
	static void convert( TReg56& _dst, const TInt32& _src )		{ _dst.var = TReg56::MyType(_src)<<24; }
	static void convert( TReg56& _dst, const TInt64& _src )		{ _dst.var = _src & TReg56::bitMask; }
	static void convert( TReg56& _dst, const TUInt64& _src )	{ _dst.var = _src & TReg56::bitMask; }

	// Move to 24-bit register
	static void convert( TReg24& _dst, TReg8 _src )				{ _dst.var = _src.signextend<int>()&0x00ffffff; }
	static void convert( TReg24& _dst, TReg24 _src )			{ _dst.var = _src.var; }
	static void convert( TReg24& _dst, TWord _src )				{ _dst.var = _src; }

	// Move to accumulator extension register
	static void convert( TReg8& _dst, TReg8 _src )				{ _dst.var = _src.var; }
	static void convert( TReg8& _dst, TReg24 _src )				{ _dst.var = (TReg8::MyType)(_src.var&TReg8::bitMask); }
	static void convert( TReg8& _dst, TReg8::MyType _src )		{ _dst.var = _src&TReg8::bitMask; }

	// Move to memory
	static void convert( TWord& _dst, TReg24 _src )				{ _dst = _src.var; }
	static void convert( TWord& _dst, TReg8 _src )				{ _dst = _src.var; }

	static void convert( TReg5& _dst, TReg5::MyType _src )		{ _dst.var = _src & TReg5::bitMask; }


//	TReg56 operator & (const TReg56& a, const TReg56& b )	{ return TReg56(a.var & b.var); }
	enum EMemArea
	{
		MemArea_X,
		MemArea_Y,
		MemArea_P,

		MemArea_COUNT,
	};
}
