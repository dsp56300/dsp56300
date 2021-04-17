#pragma once

namespace dsp56k
{
	// Address Attribute Registers
	enum AARRegisters
	{
		M_AAR0 = 0xFFFFF9,	// Address Attribute Register 0
		M_AAR1 = 0xFFFFF8,	// Address Attribute Register 1
		M_AAR2 = 0xFFFFF7,	// Address Attribute Register 2
		M_AAR3 = 0xFFFFF6,	// Address Attribute Register 3
	};

	// Address Attribute Register Bits
	enum AARBits
	{
		M_BAT = 0x3,		// External Access Type and Pin Definition Bits Mask (BAT0-BAT1)
		M_BAT0 = 0,			// External Access Type and Pin Definition Bits 0
		M_BAT1 = 1,			// External Access Type and Pin Definition Bits 1

		M_BAAP = 2,			// Address Attribute Pin Polarity
		M_BPEN = 3,			// Program Space Enable
		M_BXEN = 4,			// X Data Space Enable
		M_BYEN = 5,			// Y Data Space Enable
		M_BAM = 6,			// Address Muxing
		M_BPAC = 7,			// Packing Enable

		M_BNC = 0xF00,		// Number of Address Bits to Compare Mask (BNC0-BNC3)
		M_BNC0 = 8,			// Number of Address Bits to Compare 0
		M_BNC1 = 9,			// Number of Address Bits to Compare 1
		M_BNC2 = 10,		// Number of Address Bits to Compare 2
		M_BNC3 = 11,		// Number of Address Bits to Compare 3

		M_BAC = 0xFFF000,	// Address to Compare Bits Mask (BAC0-BAC11)
		M_BAC0 = 12,		// Address to Compare Bits 0
		M_BAC1 = 13,		// Address to Compare Bits 1
		M_BAC2 = 14,		// Address to Compare Bits 2
		M_BAC3 = 15,		// Address to Compare Bits 3
		M_BAC4 = 16,		// Address to Compare Bits 4
		M_BAC5 = 17,		// Address to Compare Bits 5
		M_BAC6 = 18,		// Address to Compare Bits 6
		M_BAC7 = 19,		// Address to Compare Bits 7
		M_BAC8 = 20,		// Address to Compare Bits 8
		M_BAC9 = 21,		// Address to Compare Bits 9
		M_BAC10 = 22,		// Address to Compare Bits 10
		M_BAC11 = 23,		// Address to Compare Bits 11
	};
}
