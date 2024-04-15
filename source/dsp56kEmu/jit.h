#pragma once

#include <memory>

#include "jitcacheentry.h"
#include "types.h"

#include <set>
#include <vector>

#include "debuggerinterface.h"
#include "jitblockchain.h"
#include "jitconfig.h"
#include "jitdspmode.h"
#include "jitruntimedata.h"

namespace asmjit
{
	inline namespace _abi_1_9
	{
		class JitRuntime;
		class CodeHolder;
	}
}

namespace dsp56k
{
	struct JitBlockInfo;
	class DSP;
	class JitBlock;
	class JitProfilingSupport;
	struct JitBlockEmitter;

	class Jit final
	{
	public:
		explicit Jit(DSP& _dsp);
		~Jit();

		DSP& dsp() { return m_dsp; }

		void exec(const TWord _pc)
		{
			m_currentChain->exec(_pc);
		}

		void notifyProgramMemWrite(const TWord _offset);

		void run(TWord _pc);
		void runCheckPMemWrite(TWord _pc);
		void runCheckPMemWriteAndModeChange(TWord _pc);
		void runCheckModeChange(TWord _pc);

		const JitConfig& getConfig() const { return m_config; }
		void setConfig(const JitConfig& _config) { m_config = _config; }
		void resetHW();
		const std::map<TWord, TWord>& getLoops() const { return m_loops; }
		const std::set< TWord>& getLoopEnds() const { return m_loopEnds; }

		static TJitFunc updateRunFunc(const JitCacheEntry& e);

		auto* getRuntime() { return m_rt; }
		auto& getRuntimeData() { return m_runtimeData; }
		const auto& getVolatileP()  { return m_volatileP; }
		auto* getProfilingSupport() const { return m_profiling.get(); }

		bool isVolatileP(const TWord _pc) const
		{
			return m_volatileP.find(_pc) != m_volatileP.end();
		}

		void create(TWord _pc, bool _execute);
		void recreate(TWord _pc);

		void addLoop(const JitBlockInfo& _info);
		void addLoop(TWord _begin, TWord _end);
		void removeLoop(const JitBlockInfo& _info);
		void removeLoop(TWord _begin);

		void destroy(TWord _pc);
		void destroyToRecreate(TWord _pc);

		void checkModeChange();

		void onDebuggerAttached(DebuggerInterface& _debugger) const;

		void destroyAllBlocks();

		JitBlockEmitter* acquireEmitter();
		void releaseEmitter(JitBlockEmitter* _emitter);

		JitBlockRuntimeData* acquireBlockRuntimeData();
		void releaseBlockRuntimeData(JitBlockRuntimeData* _b);

		void onFuncsResized(const JitBlockChain& _chain) const;

	private:
		void checkPMemWrite();

		DSP& m_dsp;

		asmjit::_abi_1_9::JitRuntime* m_rt = nullptr;

		std::map<JitDspMode, std::unique_ptr<JitBlockChain>> m_chains;
		JitBlockChain* m_currentChain = nullptr;

		std::vector<TJitFunc> m_jitFuncs;
		std::set<TWord> m_volatileP;
		std::map<TWord, TWord> m_loops;
		std::set<TWord> m_loopEnds;

		std::unique_ptr<JitProfilingSupport> m_profiling;

		std::vector<JitBlockEmitter*> m_emitters;
		std::vector<JitBlockRuntimeData*> m_blockRuntimeDatas;

		JitConfig m_config;

		size_t m_maxUsedPAddress = 0;

		// the following data is accessed by JIT code at runtime, it NEEDS to be put last into this struct to be
		// able to use ARM relative addressing, see member ordering in dsp.h
		JitRuntimeData m_runtimeData;
	};
}
