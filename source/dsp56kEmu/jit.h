#pragma once

#include <vector>

#include "types.h"

#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	class DSP;
	class JitBlock;

	class Jit
	{
	public:
		explicit Jit(DSP& _dsp);
		~Jit();

		DSP& dsp() { return m_dsp; }
		void exec();

	private:
		void emit(TWord _pc);
		void destroy(JitBlock* _block);
		void destroy(TWord _pc);

		DSP& m_dsp;

		asmjit::JitRuntime m_rt;
		std::vector<JitBlock*> m_jitCache;
	};
}
