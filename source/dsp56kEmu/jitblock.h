#pragma once

#include "jitcacheentry.h"
#include "jitdspregs.h"
#include "jitdspregpool.h"
#include "jitmem.h"
#include "jitregtracker.h"
#include "jitregtypes.h"
#include "jitruntimedata.h"
#include "jitstackhelper.h"

#include <string>
#include <vector>
#include <set>

#include "jittypes.h"

#include "opcodeanalysis.h"

namespace dsp56k
{
	class DSP;

	class JitBlock final
	{
	public:
		enum JitBlockFlags
		{
			WritePMem			= 0x0002,
			LoopEnd				= 0x0004,
			InstructionLimit	= 0x0008,
		};

		typedef void (*JitEntry)(Jit*, TWord, JitBlock*);

		JitBlock(JitEmitter& _a, DSP& _dsp, JitRuntimeData& _runtimeData);

		JitEmitter& asm_() { return m_asm; }
		DSP& dsp() { return m_dsp; }
		JitStackHelper& stack() { return m_stack; }
		JitRegpool& gpPool() { return m_gpPool; }
		JitRegpool& xmmPool() { return m_xmmPool; }
		JitDspRegs& regs() { return m_dspRegs; }
		JitDspRegPool& dspRegPool() { return m_dspRegPool; }
		Jitmem& mem() { return m_mem; }

		operator JitEmitter& ()		{ return m_asm;	}

		bool emit(Jit* _jit, TWord _pc, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP);
		bool empty() const { return m_pMemSize == 0; }
		TWord getPCFirst() const { return m_pcFirst; }
		TWord getPMemSize() const { return m_pMemSize; }

		void setFunc(const JitEntry _func, size_t _codeSize) { m_func = _func; m_codeSize = _codeSize; }
		const JitEntry& getFunc() const { return m_func; }

		TWord& getEncodedInstructionCount() { return m_encodedInstructionCount; }

		// JIT code writes these
		TWord& nextPC() { return m_runtimeData.m_nextPC; }
		uint32_t& pMemWriteAddress() { return m_runtimeData.m_pMemWriteAddress; }
		uint32_t& pMemWriteValue() { return m_runtimeData.m_pMemWriteValue; }
		void setNextPC(const JitRegGP& _pc);

		const std::string& getDisasm() const { return m_dspAsm; }
		TWord getLastOpSize() const { return m_lastOpSize; }
		TWord getSingleOpWord() const { return m_singleOpWord; }
		uint32_t getFlags() const { return m_flags; }

		TWord getChild() const { return m_child; }
		size_t codeSize() const { return m_codeSize; }
		const std::set<TWord>& getParents() const { return m_parents; }

		void increaseInstructionCount(const asmjit::Operand& _count);

	private:
		void addParent(TWord _pc);

		JitEntry m_func = nullptr;
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
		TWord m_singleOpWord = 0;
		TWord m_encodedInstructionCount = 0;

		std::string m_dspAsm;
		bool m_possibleBranch = false;
		uint32_t m_flags = 0;
		TWord m_child = g_invalidAddress;			// JIT block that we call
		bool m_childIsDynamic = false;
		size_t m_codeSize = 0;

		std::set<TWord> m_parents;
	};
}
