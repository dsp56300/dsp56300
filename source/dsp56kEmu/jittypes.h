#pragma once

#include "asmjit/x86/x86assembler.h"

namespace dsp56k
{
	// TODO: asmjit should have code for this, I can't find it
#ifdef _MSC_VER
	static constexpr auto regArg0 = asmjit::x86::rcx;
#else
	static constexpr auto regArg0 = asmjit::x86::rdi;
#endif
	constexpr auto regArg1 = asmjit::x86::rdx;
	constexpr auto regArg2 = asmjit::x86::r8;
	constexpr auto regArg3 = asmjit::x86::r9;

	static constexpr auto regReturnVal = asmjit::x86::rax;

	static constexpr auto regR(const int _index) noexcept
	{
		return asmjit::x86::xmm(_index);
	}

	static constexpr auto regAlu(const int _index) noexcept
	{
		return asmjit::x86::xmm(_index + 8);
	}

	static constexpr auto regXY(const int _index) noexcept
	{
		return asmjit::x86::xmm(_index + 10);
	}

	static constexpr auto regA() noexcept { return regAlu(0); }
	static constexpr auto regB() noexcept { return regAlu(1); }

	static constexpr auto regX() noexcept { return regXY(0); }
	static constexpr auto regY() noexcept { return regXY(1); }

	enum XMMReg
	{
		xmmR0, xmmR1, xmmR2, xmmR3, xmmR4, xmmR5, xmmR6, xmmR7,
		xmmA, xmmB,
		xmmX, xmmY
	};

	enum GPReg
	{
// TODO: asmjit should have code for this, I can't find it
#ifdef _MSC_VER
		gpOP = asmjit::x86::Gp::kIdAx,	// rax
#else
		gpOP = asmjit::x86::Gp::kIdDi,	// rdi
#endif

		gpSR = asmjit::x86::Gp::kIdR8,
		gpPC = asmjit::x86::Gp::kIdR9,
		gpLC = asmjit::x86::Gp::kIdR10,
		gpLA = asmjit::x86::Gp::kIdR11,
	};

	static constexpr auto regSR = asmjit::x86::r8;
	static constexpr auto regPC = asmjit::x86::r9;
	static constexpr auto regLC = asmjit::x86::r10;
	static constexpr auto regLA = asmjit::x86::r11;

	static constexpr auto regGPTempA = asmjit::x86::r12;
	static constexpr auto regGPTempB = asmjit::x86::r13;
	static constexpr auto regGPTempC = asmjit::x86::r14;
	static constexpr auto regGPTempD = asmjit::x86::r15;

	static constexpr auto regXMMTempA = asmjit::x86::xmm13;
	static constexpr auto regXMMTempB = asmjit::x86::xmm14;
	static constexpr auto regXMMTempC = asmjit::x86::xmm15;

	static constexpr auto regLastModAlu = asmjit::x86::xmm12;

	using JitReg64 = asmjit::x86::Gpq;
	using JitReg32 = asmjit::x86::Gpd;
	using JitReg = asmjit::x86::Gp;

	using JitReg128 = asmjit::x86::Xmm;
}
