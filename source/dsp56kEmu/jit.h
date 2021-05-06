#pragma once

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"

#include "jittypes.h"

namespace dsp56k
{
	class DSP;

	class Jit
	{
	public:
		Jit(DSP& _dsp);
		~Jit();

		DSP& dsp() { return m_dsp; }

	private:
		DSP& m_dsp;

		asmjit::JitRuntime m_rt;
		asmjit::CodeHolder m_code;
		asmjit::x86::Assembler* m_asm = nullptr;
	};
}
