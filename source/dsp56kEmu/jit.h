#pragma once

#include <memory>

#include "types.h"

#include <set>
#include <string>   // MNM patch: std::string failReason (MSVC does not get it through the other headers)
#include <vector>
#include <unordered_map>

#include "debuggerinterface.h"

#include "jitblockchain.h"
#include "jitcacheentry.h"
#include "jitconfig.h"
#include "jitdspmode.h"
#include "jitruntimedata.h"

namespace asmjit
{
	inline namespace ASMJIT_ABI_NAMESPACE
	{
		class JitRuntime;
		class CodeHolder;
	}
}

namespace dsp56k
{
	struct DspRegs;
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

		static Jit* toJitPtr(DspRegs* _regs);
		void notifyProgramMemWrite(const TWord _offset);

		void run(TWord _pc) noexcept;
		void runCheckPMemWrite(TWord _pc) noexcept;
		void runCheckPMemWriteAndModeChange(TWord _pc) noexcept;
		void runCheckModeChange(TWord _pc) noexcept;

		const JitConfig& getConfig() const { return m_config; }
		JitConfig getConfig(TWord _pc) const;
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

		// MNM patch: a block that could not be generated (code generation or the JIT runtime's add() failed, e.g. no
		// JIT memory) is a sticky failure the host must check after exec(): the DSP makes no progress at that pc
		// (create() returns instead of re-entering itself through the stub, which recursed without bound before).
		bool hasFailed() const { return m_failed; }
		const std::string& failReason() const { return m_failReason; }
		void setFailure(const std::string& _reason) { if(!m_failed) { m_failed = true; m_failReason = _reason; } }

		void create(TWord _pc, bool _execute);
		void recreate(TWord _pc);

		void addLoop(const JitBlockInfo& _info);
		void addLoop(TWord _begin, TWord _end);
		void removeLoop(const JitBlockInfo& _info);
		void removeLoop(TWord _begin);

		void destroy(TWord _pc);
		void destroyToRecreate(TWord _pc);

		void checkModeChange() noexcept;

		void onDebuggerAttached(DebuggerInterface& _debugger) const;

		void destroyAllBlocks();

		JitBlockEmitter* acquireEmitter(JitConfig&& _config);
		JitBlockEmitter* acquireEmitter(TWord _pc);
		void releaseEmitter(JitBlockEmitter* _emitter);

		JitBlockRuntimeData* acquireBlockRuntimeData();
		void releaseBlockRuntimeData(JitBlockRuntimeData* _b);

		void onFuncsResized(const JitBlockChain& _chain) const;

	private:
		void checkPMemWrite() noexcept;

		DSP& m_dsp;

		asmjit::ASMJIT_ABI_NAMESPACE::JitRuntime* m_rt = nullptr;

		struct DspModeHash
		{
		    std::size_t operator () (const JitDspMode& m) const	{ return m.get(); }
		};

		std::unordered_map<JitDspMode, std::unique_ptr<JitBlockChain>, DspModeHash> m_chains;
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
		bool m_failed = false;   // MNM patch
		std::string m_failReason;
	};
}
