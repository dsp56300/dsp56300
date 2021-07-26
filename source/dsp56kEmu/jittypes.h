#pragma once

#include "buildconfig.h"

#if defined(HAVE_ARM64)
#include "asmjit/arm/a64operand.h"

namespace asmjit
{
	namespace a64
	{
		class Builder;
	}
}

namespace dsp56k
{
	using JitAssembler = asmjit::a64::Builder;

	using JitMemPtr = asmjit::arm::Mem;

	using JitReg = asmjit::arm::Reg;
	using JitRegGP = asmjit::arm::Gp;
	using JitReg32 = asmjit::arm::GpW;
	using JitReg64 = asmjit::arm::GpX;
	using JitReg128 = asmjit::arm::VecV;
}
#else
#include "asmjit/x86/x86operand.h"

namespace asmjit
{
	namespace x86
	{
		class Builder;
	}
}

namespace dsp56k
{
	using JitAssembler = asmjit::x86::Builder;

	using JitMemPtr = asmjit::x86::Mem;

	using JitReg = asmjit::x86::Reg;
	using JitRegGP = asmjit::x86::Gp;
	using JitReg32 = asmjit::x86::Gpd;
	using JitReg64 = asmjit::x86::Gpq;
	using JitReg128 = asmjit::x86::Xmm;
}
#endif
