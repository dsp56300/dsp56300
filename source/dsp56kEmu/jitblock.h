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
	class JitDspMode;

	class JitBlock final
	{
	public:
		enum JitBlockFlags
		{
			WritePMem			= 0x0002,
			LoopEnd				= 0x0004,
			ModeChange			= 0x0008
		};

		struct InstructionProfilingInfo
		{
			TWord pc = 0;
			TWord opA = 0, opB = 0;	// used for delayed disasm
			TWord opLen = 0;
			TWord lineCount = 1;
			asmjit::Label labelBefore, labelAfter;
			uint64_t codeOffset = 0;
			uint64_t codeOffsetAfter = 0;
			std::string sourceText;
		};

		static constexpr TWord SingleOpCacheIgnoreWordB = 0xffffffff;

		JitBlock(JitEmitter& _a, DSP& _dsp, JitRuntimeData& _runtimeData, const JitConfig& _config);
		~JitBlock();

		JitEmitter& asm_() { return m_asm; }
		DSP& dsp() { return m_dsp; }
		JitStackHelper& stack() { return m_stack; }
		JitRegpool& gpPool() { return m_gpPool; }
		JitRegpool& xmmPool() { return m_xmmPool; }
		JitDspRegs& regs() { return m_dspRegs; }
		JitDspRegPool& dspRegPool() { return m_dspRegPool; }
		Jitmem& mem() { return m_mem; }

		operator JitEmitter& ()		{ return m_asm;	}

		static void getInfo(JitBlockInfo& _info, const DSP& _dsp, TWord _pc, const JitConfig& _config, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds);

		bool emit(JitBlockChain* _chain, TWord _pc, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds, bool _profilingSupport);
		bool empty() const { return m_pMemSize == 0; }
		TWord getPCFirst() const { return m_pcFirst; }
		TWord getPMemSize() const { return m_pMemSize; }

		void finalize(const TJitFunc _func, const asmjit::CodeHolder& _codeHolder);

		const TJitFunc& getFunc() const { return m_func; }

		TWord& getEncodedInstructionCount() { return m_encodedInstructionCount; }

		// JIT code writes these
		TWord& nextPC() { return m_runtimeData.m_nextPC; }
		uint32_t& pMemWriteAddress() { return m_runtimeData.m_pMemWriteAddress; }
		uint32_t& pMemWriteValue() { return m_runtimeData.m_pMemWriteValue; }
		void setNextPC(const JitRegGP& _pc);
		void setNextPC(const DspValue& _pc);

		const std::string& getDisasm() const { return m_dspAsm; }
		TWord getLastOpSize() const { return m_lastOpSize; }
		TWord getSingleOpWordA() const { return m_singleOpWordA; }
		TWord getSingleOpWordB() const { return m_singleOpWordB; }

		uint64_t getSingleOpCacheKey() const
		{
			return getSingleOpCacheKey(m_singleOpWordA, getPMemSize() == 1 ? SingleOpCacheIgnoreWordB : m_singleOpWordB);
		}

		static uint64_t getSingleOpCacheKey(TWord _opA, TWord _opB);

		TWord getChild() const { return m_child; }
		TWord getNonBranchChild() const { return m_nonBranchChild; }
		size_t codeSize() const { return m_codeSize; }
		const std::set<TWord>& getParents() const { return m_parents; }

		void increaseInstructionCount(const asmjit::Operand& _count);
		void clearParents() { m_parents.clear(); }

		std::vector<InstructionProfilingInfo>& getProfilingInfo() { return m_profilingInfo; }
		size_t getCodeSize() const { return m_codeSize; }

		const JitConfig& getConfig() const { return m_config; }

		AddressingMode getAddressingMode(uint32_t _aguIndex) const;
		const JitDspMode* getMode() const;

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

		const JitBlockInfo& getInfo() const { return m_info; }

	private:
		void addParent(TWord _pc);

		class JitBlockGenerating
		{
		public:
			JitBlockGenerating(JitBlock& _block) : m_block(_block) { _block.m_generating = true; }
			~JitBlockGenerating() { m_block.m_generating = false; }
		private:
			JitBlock& m_block;
		};

		friend class JitBlockGenerating;

		TJitFunc m_func = nullptr;
		JitRuntimeData& m_runtimeData;

		JitEmitter& m_asm;
		DSP& m_dsp;
		JitStackHelper m_stack;
		JitRegpool m_xmmPool;
		JitRegpool m_gpPool;
		JitDspRegs m_dspRegs;
		JitDspRegPool m_dspRegPool;
		Jitmem m_mem;

		TWord m_pcFirst = 0;
		TWord m_pMemSize = 0;
		TWord m_lastOpSize = 0;
		TWord m_singleOpWordA = 0;
		TWord m_singleOpWordB = 0;
		TWord m_encodedInstructionCount = 0;

		std::string m_dspAsm;
		bool m_possibleBranch = false;
		TWord m_child = g_invalidAddress;			// JIT block that we call
		TWord m_nonBranchChild = g_invalidAddress;
		size_t m_codeSize = 0;

		JitBlockInfo m_info;

		std::set<TWord> m_parents;
		bool m_generating = false;
		std::vector<InstructionProfilingInfo> m_profilingInfo;
		const JitConfig& m_config;
		JitBlockChain* m_chain = nullptr;

		bool m_scratchLocked = false;
	};
}
