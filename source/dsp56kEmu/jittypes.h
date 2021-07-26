#pragma once

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

	using JitReg = asmjit::x86::Reg;
	using JitRegGP = asmjit::x86::Gp;
	using JitReg32 = asmjit::x86::Gpd;
	using JitReg64 = asmjit::x86::Gpq;
	using JitReg128 = asmjit::x86::Xmm;
}
