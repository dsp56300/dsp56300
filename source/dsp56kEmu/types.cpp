#include "pch.h"

#include "types.h"

namespace dsp56k
{
	const TReg5::MyType TReg5::bitCount = 5;
	const TReg5::MyType TReg5::bitMask  = 0x1f;

	const TReg8::MyType TReg8::bitCount = 8;
	const TReg8::MyType TReg8::bitMask  = 0xff;

	const TReg24::MyType TReg24::bitCount = 24;
	const TReg24::MyType TReg24::bitMask  = 0x00ffffff;

	const TReg48::MyType TReg48::bitCount = 48;
	const TReg48::MyType TReg48::bitMask  = 0x0000ffffffffffff;

	const TReg56::MyType TReg56::bitCount = 56;
	const TReg56::MyType TReg56::bitMask  = 0x00ffffffffffffff;

	const char g_memAreaNames[MemArea_COUNT] = { 'X', 'Y', 'P' };
}

