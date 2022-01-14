#pragma once

#include "jittypes.h"

namespace dsp56k
{
#if defined(HAVE_ARM64)
	// We do NOT use the following GP registers:
	// X8 = XR
	// X16 = IP0
	// X17 = IP1
	// X18 = PR
	// X29 = FP
	// X30 = LR

	// Furthermore, we have so many GPs, we do not use the ones that are callee-save. We use vector registers instead to prevent that we have to push/pop when calling C functions

	static constexpr JitReg64 g_funcArgGPs[] = {JitReg64(0), JitReg64(1), JitReg64(2), JitReg64(3)};

	static constexpr JitReg64 g_nonVolatileGPs[] = {JitReg64(19), JitReg64(20), JitReg64(21), JitReg64(22), JitReg64(23), JitReg64(24), JitReg64(25), JitReg64(26), JitReg64(27), JitReg64(28) };

	static constexpr JitReg128 g_nonVolatileXMMs[] = {JitReg128()};	// none

	static constexpr JitRegGP g_dspPoolGps[] = {JitReg64(2), JitReg64(3), JitReg64(4), JitReg64(5), JitReg64(6), JitReg64(7), JitReg64(9)};

	static constexpr auto regReturnVal = JitReg64(0);

	static constexpr auto regDspPtr = JitReg64(1);

	// compared to X64, we use one additional temp because we do not have a fixed shift register, which leads to one additional temp register
	static constexpr std::initializer_list<JitReg> g_regGPTemps = { JitReg64(10), JitReg64(11), JitReg64(12), JitReg64(13), JitReg64(14), JitReg64(15) };

	static constexpr auto regLastModAlu = JitReg128(0);

	static constexpr auto regXMMTempA = JitReg128(1);

	static constexpr JitReg128 g_dspPoolXmms[] = {                                 JitReg128(2) ,  JitReg128(3) , JitReg128(4) , JitReg128(5) , JitReg128(6) , JitReg128(7),
												   JitReg128(8) , JitReg128(9) ,  JitReg128(10),  JitReg128(11), JitReg128(12), JitReg128(13), JitReg128(14), JitReg128(15),
												   JitReg128(16), JitReg128(17),  JitReg128(18),  JitReg128(19), JitReg128(20), JitReg128(21), JitReg128(22), JitReg128(23),
												   JitReg128(24), JitReg128(25),  JitReg128(26),  JitReg128(27), JitReg128(28), JitReg128(29), JitReg128(30), JitReg128(31) };
#else
#ifdef _MSC_VER
	static constexpr JitReg64 g_funcArgGPs[] = {asmjit::x86::rcx, asmjit::x86::rdx, asmjit::x86::r8, asmjit::x86::r9};

	static constexpr JitReg64 g_nonVolatileGPs[] = { asmjit::x86::rbx, asmjit::x86::rbp, asmjit::x86::rdi, asmjit::x86::rsi, asmjit::x86::rsp
	                                               , asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::r14, asmjit::x86::r15};

	static constexpr JitReg128 g_nonVolatileXMMs[] = { asmjit::x86::xmm6, asmjit::x86::xmm7, asmjit::x86::xmm8, asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11, asmjit::x86::xmm12, asmjit::x86::xmm13, asmjit::x86::xmm14, asmjit::x86::xmm15 };

	static constexpr JitRegGP g_dspPoolGps[] = { asmjit::x86::rdx, asmjit::x86::r8, asmjit::x86::r9, asmjit::x86::r10, asmjit::x86::r11, asmjit::x86::rbx, asmjit::x86::rdi};

	static constexpr auto regDspPtr = asmjit::x86::rsi;
#else
	static constexpr JitReg64 g_funcArgGPs[] = { asmjit::x86::rdi, asmjit::x86::rsi, asmjit::x86::rdx, asmjit::x86::rcx };

	// Note: rcx is not used in any pools because it is needed as shift register

	static constexpr JitReg64 g_nonVolatileGPs[] = { asmjit::x86::rbx, asmjit::x86::rbp, asmjit::x86::rsi, asmjit::x86::rsp
	                                               , asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::r14, asmjit::x86::r15};
	
	static constexpr JitReg128 g_nonVolatileXMMs[] = { asmjit::x86::xmm6, asmjit::x86::xmm7, asmjit::x86::xmm8, asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11, asmjit::x86::xmm12, asmjit::x86::xmm13, asmjit::x86::xmm14, asmjit::x86::xmm15 };

	static constexpr JitRegGP g_dspPoolGps[] = { asmjit::x86::rdx, asmjit::x86::r8, asmjit::x86::r9, asmjit::x86::r10, asmjit::x86::r11, asmjit::x86::rsi, asmjit::x86::rdi };
	
	static constexpr auto regDspPtr = asmjit::x86::rbx;
#endif

	static constexpr auto regReturnVal = asmjit::x86::rax;

	static constexpr std::initializer_list<JitReg> g_regGPTemps = { asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::r14, asmjit::x86::r15, asmjit::x86::rbp };

	static constexpr auto regLastModAlu = asmjit::x86::xmm0;

	static constexpr auto regXMMTempA = asmjit::x86::xmm1;

	static constexpr JitReg128 g_dspPoolXmms[] =	{ asmjit::x86::xmm2, asmjit::x86::xmm3,  asmjit::x86::xmm4,  asmjit::x86::xmm5,  asmjit::x86::xmm6,  asmjit::x86::xmm7,  asmjit::x86::xmm8,
													  asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11, asmjit::x86::xmm12, asmjit::x86::xmm13, asmjit::x86::xmm14, asmjit::x86::xmm15};
#endif
}
