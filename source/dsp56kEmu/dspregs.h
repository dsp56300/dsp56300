#pragma once

#include "types.h"

namespace dsp56k
{
	struct DspRegs
	{
		// accumulator
		TReg48 x,y;						// 48 bit
		// 56 bit accumulators, stored LEFT-ALIGNED: the value sits in bits 63..8 and bits 7..0 are
		// ALWAYS ZERO. The DSP56300 has no bits below the accumulator, so anything reaching them would
		// be resolution the hardware never had - keeping them clear is a correctness contract, not an
		// optimisation. The payoff is that the JIT loads and stores these with a plain mov.
		// Both the JIT and the interpreter work in this representation natively; only DSP::aluA()/aluB()
		// and DSP::setALU() convert, and they exist purely for readReg/writeReg, the debugger and tests.
		TReg56 a, b;
	
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
