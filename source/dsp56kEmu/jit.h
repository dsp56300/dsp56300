#pragma once

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	class DSP;

	class Jit
	{
	public:
		Jit(DSP& _dsp);
		~Jit();

	private:
		DSP& m_dsp;

		asmjit::JitRuntime m_rt;
		asmjit::CodeHolder m_code;
	};
}
