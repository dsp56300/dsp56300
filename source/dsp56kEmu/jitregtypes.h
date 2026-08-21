#pragma once

#include "jittypes.h"

namespace dsp56k
{
#if defined(HAVE_ARM64)
	// https://developer.arm.com/documentation/102374/0100/Procedure-Call-Standard
	// X8 = XR
	// X16 = IP0
	// X17 = IP1
	// X18 = PR
	// X29 = FP
	// X30 = LR

	// Furthermore, we have so many GPs, we do not use the ones that are callee-save. We use vector registers instead to prevent that we have to push/pop when calling C functions

	static constexpr JitReg64 g_funcArgGPs[] = {JitReg64(0), JitReg64(1), JitReg64(2), JitReg64(3), JitReg64(4), JitReg64(5), JitReg64(6), JitReg64(7)};

	static constexpr JitReg64 g_nonVolatileGPs[] = {JitReg64(18), JitReg64(19), JitReg64(20), JitReg64(21), JitReg64(22), JitReg64(23), JitReg64(24), JitReg64(25), JitReg64(26), JitReg64(27), JitReg64(28), JitReg64(29)/*, JitReg64(30)*/ };

	static constexpr JitReg128 g_nonVolatileXMMs[] = {JitReg128(8) , JitReg128(9) ,  JitReg128(10),  JitReg128(11), JitReg128(12), JitReg128(13), JitReg128(14), JitReg128(15)};

	static constexpr JitRegGP g_dspPoolGps[] = { JitReg64(1), JitReg64(2), JitReg64(3), JitReg64(4), JitReg64(5), JitReg64(6), JitReg64(7), JitReg64(8), JitReg64(9), JitReg64(14), JitReg64(15), JitReg64(16), JitReg64(17)};

	static constexpr auto regReturnVal = JitReg64(0);

	static constexpr auto regDspPtr = JitReg64(20);

	// compared to X64, we use one additional temp because we do not have a fixed shift register, which leads to one additional temp register
	static constexpr std::initializer_list<JitReg> g_regGPTemps = { JitReg64(10), JitReg64(11), JitReg64(12), JitReg64(13) };

	static constexpr auto regLastModAlu = JitReg128(0);

	static constexpr auto regXMMTempA = JitReg128(1);

	// ARM64 keeps 22 volatile spill slots, so withholding the 8 callee-saved ones only removes capacity that
	// some blocks genuinely need - measured flat overall but a reproducible -4% on one workload. Keep them.
	static constexpr bool g_spillToNonVolatileXmms = true;

	// The trampoline could take the callee-saved vector registers over as well - blocks spill into them
	// and push the union they might need even on paths that never use them. Measured on Cortex-A76 that
	// is a loss: -1.0% despite removing 2.2% of all executed instructions, with perf blaming frontend
	// stalls inside the blocks. The trampoline loop on its own is free there (-0.05%, n=24), so the loop
	// stays and the vectors are left to the blocks. On Win64 it is a loss too, see g_trampolineSavedGPs.

	static constexpr JitReg128 g_dspPoolXmms[] = {                                JitReg128(2) ,  JitReg128(3) , JitReg128(4) , JitReg128(5) , JitReg128(6) , JitReg128(7) ,
												   JitReg128(16), JitReg128(17),  JitReg128(18),  JitReg128(19), JitReg128(20), JitReg128(21), JitReg128(22), JitReg128(23),
												   JitReg128(24), JitReg128(25),  JitReg128(26),  JitReg128(27), JitReg128(28), JitReg128(29), JitReg128(30), JitReg128(31),
	/* we use the non-volatile ones last */        JitReg128(8) , JitReg128(9) ,  JitReg128(10),  JitReg128(11), JitReg128(12), JitReg128(13), JitReg128(14), JitReg128(15) };
#else
#ifdef _MSC_VER
	static constexpr JitReg64 g_funcArgGPs[] = {asmjit::x86::rcx, asmjit::x86::rdx, asmjit::x86::r8, asmjit::x86::r9};

	static constexpr JitReg64 g_nonVolatileGPs[] = { asmjit::x86::rbx, asmjit::x86::rbp, asmjit::x86::rdi, asmjit::x86::rsi, asmjit::x86::rsp
	                                               , asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::r14, asmjit::x86::r15};

	static constexpr JitReg128 g_nonVolatileXMMs[] = { asmjit::x86::xmm6, asmjit::x86::xmm7, asmjit::x86::xmm8, asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11, asmjit::x86::xmm12, asmjit::x86::xmm13, asmjit::x86::xmm14, asmjit::x86::xmm15 };

	static constexpr JitRegGP g_dspPoolGps[] = { asmjit::x86::rdx, asmjit::x86::r8, asmjit::x86::r9, asmjit::x86::r11, asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::rsi, asmjit::x86::rbp, asmjit::x86::rdi};

	static constexpr auto regDspPtr = asmjit::x86::rbx;
#else
	static constexpr JitReg64 g_funcArgGPs[] = { asmjit::x86::rdi, asmjit::x86::rsi, asmjit::x86::rdx, asmjit::x86::rcx, asmjit::x86::r8, asmjit::x86::r9 };

	// Note: rcx is not used in any pools because it is needed as shift register

	static constexpr JitReg64 g_nonVolatileGPs[] = { asmjit::x86::rbx, asmjit::x86::rbp, asmjit::x86::rsp
	                                               , asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::r14, asmjit::x86::r15};
	
	static constexpr JitReg128 g_nonVolatileXMMs[] = {};

	// their register set (rbx becomes regDspPtr and leaves the pool, r8 joins it) with the
	// volatiles-first ordering, see static_assert in jit.cpp
	static constexpr JitRegGP g_dspPoolGps[] = { asmjit::x86::rdx, asmjit::x86::rsi, asmjit::x86::rdi, asmjit::x86::r8, asmjit::x86::r9, asmjit::x86::r11, asmjit::x86::rbp, asmjit::x86::r12, asmjit::x86::r13 };
	
	static constexpr auto regDspPtr = asmjit::x86::rbx;
#endif

	// x86-64 has only 4 volatile spill slots, so blocks reach into the callee-saved XMMs readily and pay for it
	// twice: once in prolog/epilog save-restore, and again in spill churn the pool would not otherwise do.
	// Withholding them cuts spill moves by 29% and memory ops by 14%, worth +1.04% MIPS on Windows.
	// No effect on SysV, where every pool XMM is already volatile.
	static constexpr bool g_spillToNonVolatileXmms = false;

	// Callee-saved GPs that blocks use - either through the pool or as g_regGPTemps scratch. The
	// trampoline saves these ONCE around a whole batch instead of every block pushing and popping
	// its own (measured at 9.4% of all emitted host instructions). Blocks may treat them as volatile.
	// Worth +5.65% on SysV; on Win64 it measures flat (+0.48%, n.s.) and adding the callee-saved XMMs
	// on top costs -3.4%: with g_spillToNonVolatileXmms off, xmm6-15 are not in the pool at all, so
	// hoisting them saves no prolog and only buys 10 extra saves per interrupt entry in execOne.
	// regDspPtr is deliberately NOT here: no block touches it, and the trampoline keeps it live.
	// EVERY entry into a block must go through JitTrampoline for this to hold.
#ifdef _MSC_VER
	static constexpr JitReg64 g_trampolineSavedGPs[] = { asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::rsi, asmjit::x86::rbp, asmjit::x86::rdi, asmjit::x86::r14, asmjit::x86::r15 };
#else
	static constexpr JitReg64 g_trampolineSavedGPs[] = { asmjit::x86::rbp, asmjit::x86::r12, asmjit::x86::r13, asmjit::x86::r14, asmjit::x86::r15 };
#endif

	static constexpr auto regReturnVal = asmjit::x86::rax;

	static constexpr std::initializer_list<JitReg> g_regGPTemps = { asmjit::x86::r10, asmjit::x86::r14, asmjit::x86::r15};

	static constexpr auto regLastModAlu = asmjit::x86::xmm0;

	static constexpr auto regXMMTempA = asmjit::x86::xmm1;

	static constexpr JitReg128 g_dspPoolXmms[] =	{ asmjit::x86::xmm2, asmjit::x86::xmm3,  asmjit::x86::xmm4,  asmjit::x86::xmm5,  asmjit::x86::xmm6,  asmjit::x86::xmm7,  asmjit::x86::xmm8,
													  asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11, asmjit::x86::xmm12, asmjit::x86::xmm13, asmjit::x86::xmm14, asmjit::x86::xmm15};
#endif
}
