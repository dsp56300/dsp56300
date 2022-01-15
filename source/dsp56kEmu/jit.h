#pragma once

#include "jitcacheentry.h"
#include "types.h"

#include <vector>
#include <set>

#include "jitruntimedata.h"

namespace asmjit
{
	inline namespace _abi_1_8
	{
		class JitRuntime;
	}
}

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
		void runCheckLoopEndAndPMemWrite(TWord _pc, JitBlock* _block);
		void create(TWord _pc, JitBlock* _block);
		void recreate(TWord _pc, JitBlock* _block);

	private:
		void emit(TWord _pc);
		void destroy(JitBlock* _block);
		void destroy(TWord _pc);
		void release(const JitBlock* _block);
		
		void exec(TWord pc, JitCacheEntry& e);

		static void updateRunFunc(JitCacheEntry& e);

		void checkPMemWrite(TWord _pc, JitBlock* _block);
		void checkLoopEnd(TWord _pc, JitBlock* _block);

		JitRuntimeData m_runtimeData;

		DSP& m_dsp;

		asmjit::_abi_1_8::JitRuntime* m_rt = nullptr;
		std::vector<JitCacheEntry> m_jitCache;
		std::set<TWord> m_volatileP;
		size_t m_codeSize = 0;
	};
}
