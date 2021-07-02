#pragma once

#include "jitcacheentry.h"
#include "types.h"

#include <vector>
#include <set>

#include "jitruntimedata.h"
#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	class DSP;
	class JitBlock;

	class Jit final
	{
	public:
		explicit Jit(DSP& _dsp);
		~Jit();

		DSP& dsp() { return m_dsp; }

		void exec(TWord pc);

		void notifyProgramMemWrite(TWord _offset);

		void run(TWord _pc, JitBlock* _block);
		void runCheckPMemWrite(TWord _pc, JitBlock* _block);
		void runCheckLoopEnd(TWord _pc, JitBlock* _block);
		void create(TWord _pc, JitBlock* _block);
		void recreate(TWord _pc, JitBlock* _block);

	private:
		void emit(TWord _pc);
		void destroy(JitBlock* _block);
		void destroy(TWord _pc);
		
		void exec(TWord pc, JitCacheEntry& e);

		static void updateRunFunc(JitCacheEntry& e);

		DSP& m_dsp;

		asmjit::JitRuntime m_rt;
		std::vector<JitCacheEntry> m_jitCache;
		std::set<TWord> m_volatileP;
		JitRuntimeData m_runtimeData;
	};
}
