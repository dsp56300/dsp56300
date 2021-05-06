#pragma once

#include "dsp.h"

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"

namespace dsp56k
{
	class JitUnittests
	{
	public:
		JitUnittests();
		~JitUnittests();

	private:
		DefaultMemoryValidator m_defaultMemoryValidator;
		Peripherals56303 peripherals;
		Memory mem;
		DSP dsp;

		asmjit::JitRuntime m_rt;
		asmjit::CodeHolder m_code;
		std::unique_ptr<asmjit::x86::Assembler> m_asm = nullptr;
	};
}
