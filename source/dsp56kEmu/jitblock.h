#pragma once

#include "jitcacheentry.h"
#include "jitdspregs.h"
#include "jitdspregpool.h"
#include "jitmem.h"
#include "jitregtracker.h"
#include "jitruntimedata.h"
#include "jitstackhelper.h"
#include "jitblockinfo.h"
#include "jittypes.h"

#include "opcodeanalysis.h"

#include <string>
#include <vector>
#include <set>

namespace asmjit
{
	inline namespace _abi_1_9
	{
		class CodeHolder;
	}
}

namespace dsp56k
{
	struct JitBlockInfo;
	struct JitConfig;

	class DSP;
	class JitBlockChain;
	class JitBlockRuntimeData;
	class JitDspMode;

	class JitBlock final
	{
	public:

		JitBlock(JitEmitter& _a, DSP& _dsp, JitRuntimeData& _runtimeData, const JitConfig& _config);
		~JitBlock();

		static void getInfo(JitBlockInfo& _info, const DSP& _dsp, TWord _pc, const JitConfig& _config, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds);

		bool emit(JitBlockRuntimeData& _rt, JitBlockChain* _chain, TWord _pc, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds, bool _profilingSupport);

		JitEmitter& asm_() { return m_asm; }
		DSP& dsp() { return m_dsp; }
		JitStackHelper& stack() { return m_stack; }
		JitRegpool& gpPool() { return m_gpPool; }
		JitRegpool& xmmPool() { return m_xmmPool; }
		JitDspRegs& regs() { return m_dspRegs; }
		JitDspRegPool& dspRegPool() { return m_dspRegPool; }
		Jitmem& mem() { return m_mem; }

		operator JitEmitter& ()		{ return m_asm;	}

		// JIT code writes these
		TWord& nextPC() { return m_runtimeData.m_nextPC; }
		uint32_t& pMemWriteAddress() { return m_runtimeData.m_pMemWriteAddress; }
		uint32_t& pMemWriteValue() { return m_runtimeData.m_pMemWriteValue; }
		void setNextPC(const DspValue& _pc);

		void increaseInstructionCount(const asmjit::Operand& _count);

		const JitConfig& getConfig() const { return m_config; }

		AddressingMode getAddressingMode(uint32_t _aguIndex) const;
		const JitDspMode* getMode() const;
		void setMode(JitDspMode* _mode);

		void lockScratch()
		{
			assert(!m_scratchLocked && "scratch reg is already locked");
			m_scratchLocked = true;
		}

		void unlockScratch()
		{
			assert(m_scratchLocked && "scratch reg is not locked");
			m_scratchLocked = false;
		}

		void reset();

	private:
		class JitBlockGenerating
		{
		public:
			JitBlockGenerating(JitBlockRuntimeData& _block);
			~JitBlockGenerating();
		private:
			JitBlockRuntimeData& m_block;
		};

		JitRuntimeData& m_runtimeData;

		JitEmitter& m_asm;
		DSP& m_dsp;
		JitStackHelper m_stack;
		JitRegpool m_xmmPool;
		JitRegpool m_gpPool;
		JitDspRegs m_dspRegs;
		JitDspRegPool m_dspRegPool;
		Jitmem m_mem;

		const JitConfig& m_config;
		JitBlockChain* m_chain = nullptr;

		bool m_scratchLocked = false;

		JitDspMode* m_mode = nullptr;
	};
}
