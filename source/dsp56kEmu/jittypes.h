#pragma once

#include "asmjit/x86/x86assembler.h"

namespace dsp56k
{
#ifdef _MSC_VER
	static constexpr auto regOP = asmjit::x86::rcx;
#else
	static constexpr auto regOP = asmjit::x86::rdi;
#endif
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
#ifdef _MSC_VER
		gpOP = 2,	// rax
#else
		gpOP = 6,	// rdi
#endif
		gpSR = 8,
		gpPC = 9,
		gpLC = 10,
		gpLA = 11,
	};
}
