#pragma once

#include "types.h"

namespace dsp56k
{
	struct DspRegs
	{
		// accumulator
		TReg48 x,y;						// 48 bit
		TReg56 a,b;						// 56 bit
	
		// ---- AGU ----
		std::array<TReg24, 8> r;
		std::array<TReg24, 8> n;
		std::array<TReg24, 8> m;
		std::array<TWord, 8> mMask;
		std::array<TWord, 8> mModulo;
	
		// ---- PCU ----
		TReg24 sr;						// status register (SR_..)
		TReg24 omr;						// operation mode register
	
		TReg24 pc;						// program counter
		TReg24 la, lc;					// loop address, loop counter
	
		//TReg24	ssh, ssl;			// system stack high, system stack low
		TReg24	sp;						// stack pointer
		TReg5	sc;						// stack counter
	
		std::array<TReg48, 16> ss;		// system stack
	
		TReg24	sz;						// stack size (used for stack extension)
	
		TReg24 vba;						// vector base address
	
		TReg24 ep;						// stack extension pointer register
	};	
}
