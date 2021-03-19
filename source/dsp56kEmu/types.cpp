#include "pch.h"

#include "types.h"

namespace dsp56k
{
	const unsigned char TReg5::bitCount = 5;
	const unsigned char TReg5::bitMask  = 0x1f;

	const unsigned char TReg8::bitCount = 8;
	const unsigned char TReg8::bitMask  = 0xff;

	const int TReg24::bitCount = 24;
	const int TReg24::bitMask  = 0x00ffffff;

	const __int64 TReg48::bitCount = 48;
	const __int64 TReg48::bitMask  = 0x0000ffffffffffff;

	const __int64 TReg56::bitCount = 56;
	const __int64 TReg56::bitMask  = 0x00ffffffffffffff;
}
