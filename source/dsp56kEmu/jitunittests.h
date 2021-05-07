#pragma once

#include "dsp.h"

#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	class JitBlock;
	class JitOps;

	class JitUnittests
	{
	public:
		JitUnittests();

	private:
		void runTest(void(JitUnittests::*_build)(JitBlock&, JitOps&), void( JitUnittests::*_verify)());

		void abs_build(JitBlock& _block, JitOps& _ops);
		void abs_verify();

		void conversion_build(JitBlock& _block, JitOps& _ops);
		void conversion_verify();

		void signextend_build(JitBlock& _block, JitOps& _ops);
		void signextend_verify();

		DefaultMemoryValidator m_defaultMemoryValidator;
		Peripherals56303 peripherals;
		Memory mem;
		DSP dsp;

		asmjit::JitRuntime m_rt;

		std::array<uint64_t,32> m_checks;
	};
}
