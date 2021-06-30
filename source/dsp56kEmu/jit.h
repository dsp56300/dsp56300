#pragma once

#include "jitcacheentry.h"
#include "types.h"

#include <vector>
#include <set>

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

		void notifyProgramMemWrite(TWord _offset);

	private:
		void emit(TWord _pc);
		void destroy(JitBlock* _block);
		void destroy(TWord _pc);
		void markInvalid(JitBlock* _block);
		
		void run(TWord _pc, JitBlock* _block);
		void runCheckPMemWrite(TWord _pc, JitBlock* _block);
		void create(TWord _pc, JitBlock* _block);
		void recreate(TWord _pc, JitBlock* _block);

		DSP& m_dsp;

		asmjit::JitRuntime m_rt;
		std::vector<JitCacheEntry> m_jitCache;
		std::set<TWord> m_volatileP;
	};
}
