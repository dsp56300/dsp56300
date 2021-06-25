#pragma once

#include "asmjit/x86/x86assembler.h"

namespace dsp56k
{
	using JitReg64 = asmjit::x86::Gpq;
	using JitReg32 = asmjit::x86::Gpd;
	using JitReg = asmjit::x86::Gp;

	using JitReg128 = asmjit::x86::Xmm;

	// TODO: asmjit should have code for this, I can't find it
#ifdef _MSC_VER
	static constexpr auto regArg0 = asmjit::x86::rcx;
	static constexpr auto regArg1 = asmjit::x86::rdx;
	static constexpr auto regArg2 = asmjit::x86::r8;
	static constexpr auto regArg3 = asmjit::x86::r9;

	static constexpr auto regPoolA = asmjit::x86::rdx;
	static constexpr auto regPoolB = asmjit::x86::r8;
	static constexpr auto regPoolC = asmjit::x86::r9;
	static constexpr auto regPoolD = asmjit::x86::rsi;
	static constexpr auto regPoolE = asmjit::x86::rdi;
#else
	static constexpr auto regArg0 = asmjit::x86::rdi;
	static constexpr auto regArg1 = asmjit::x86::rsi;
	static constexpr auto regArg2 = asmjit::x86::rdx;
	static constexpr auto regArg3 = asmjit::x86::rcx;

	static constexpr auto regPoolA = asmjit::x86::rsi;
	static constexpr auto regPoolB = asmjit::x86::rdx;
	static constexpr auto regPoolC = asmjit::x86::rcx;
	static constexpr auto regPoolD = asmjit::x86::r8;
	static constexpr auto regPoolE = asmjit::x86::r9;
#endif

	static constexpr auto regReturnVal = asmjit::x86::rax;

	static constexpr auto regLC = asmjit::x86::r10;
	static constexpr auto regExtMem = asmjit::x86::r11;

	static constexpr auto regGPTempA = asmjit::x86::r12;
	static constexpr auto regGPTempB = asmjit::x86::r13;
	static constexpr auto regGPTempC = asmjit::x86::r14;
	static constexpr auto regGPTempD = asmjit::x86::r15;
	static constexpr auto regGPTempE = asmjit::x86::rbp;

	static constexpr auto regXMMTempA = asmjit::x86::xmm13;
	static constexpr auto regXMMTempB = asmjit::x86::xmm14;
	static constexpr auto regXMMTempC = asmjit::x86::xmm15;

	static constexpr auto regLastModAlu = asmjit::x86::xmm12;

	static constexpr JitReg g_dspPoolGps[] =	{ regPoolA, regPoolB, regPoolC, regPoolD, regPoolE, asmjit::x86::r10, asmjit::x86::r11};

	static constexpr JitReg128 g_dspPoolXmms[] =	{ asmjit::x86::xmm6, asmjit::x86::xmm7, asmjit::x86::xmm8, asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11
													, asmjit::x86::xmm0, asmjit::x86::xmm1, asmjit::x86::xmm2, asmjit::x86::xmm3, asmjit::x86::xmm4, asmjit::x86::xmm5};

}
