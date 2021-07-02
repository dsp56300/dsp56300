#pragma once

#include "jitcacheentry.h"
#include "jitdspregs.h"
#include "jitdspregpool.h"
#include "jitmem.h"
#include "jitregtracker.h"
#include "jitregtypes.h"
#include "jitstackhelper.h"

#include <string>
#include <vector>
#include <set>

namespace asmjit
{
	namespace x86
	{
		class Assembler;
	}
}

namespace dsp56k
{
	class DSP;

	constexpr TWord g_pcInvalid = 0xffffffff;

	class JitBlock final
	{
	public:
		enum JitBlockFlags
		{
			Failed		= 0,
			Success		= 0x0001,
			WritePMem	= 0x0002,
			LoopEnd		= 0x0004,
		};

		typedef void (*JitEntry)();

		JitBlock(asmjit::x86::Assembler& _a, DSP& _dsp);

		asmjit::x86::Assembler& asm_() { return m_asm; }
		DSP& dsp() { return m_dsp; }
		JitStackHelper& stack() { return m_stack; }
		JitRegpool& gpPool() { return m_gpPool; }
		JitRegpool& xmmPool() { return m_xmmPool; }
		JitDspRegs& regs() { return m_dspRegs; }
		JitDspRegPool& dspRegPool() { return m_dspRegPool; }
		Jitmem& mem() { return m_mem; }

		operator asmjit::x86::Assembler& ()		{ return m_asm;	}

		bool emit(TWord _pc, std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP);
		bool empty() const { return m_pMemSize == 0; }
		TWord getPCFirst() const { return m_pcFirst; }
		TWord getPMemSize() const { return m_pMemSize; }

		void setFunc(const JitEntry _func) { m_func = _func; }
		const JitEntry& getFunc() const { return m_func; }

		void exec();
		TWord& getEncodedInstructionCount() { return m_encodedInstructionCount; }
		const TWord& getExecutedInstructionCount() const { return m_executedInstructionCount; }

		// JIT code writes these
		TWord& nextPC() { return m_nextPC; }
		uint32_t& pMemWriteAddress() { return m_pMemWriteAddress; }
		uint32_t& pMemWriteValue() { return m_pMemWriteValue; }
		void setNextPC(const JitReg& _pc);

		const std::string& getDisasm() const { return m_dspAsm; }
		TWord getLastOpSize() const { return m_lastOpSize; }
		TWord getSingleOpWord() const { return m_singleOpWord; }
		uint32_t getFlags() const { return m_flags; }

	private:
		JitEntry m_func = nullptr;

		asmjit::x86::Assembler& m_asm;
		DSP& m_dsp;
		JitStackHelper m_stack;
		JitRegpool m_xmmPool;
		JitRegpool m_gpPool;
		JitDspRegs m_dspRegs;
		JitDspRegPool m_dspRegPool;
		Jitmem m_mem;

		TWord m_pcFirst = 0;
		TWord m_pcLast = 0;
		TWord m_pMemSize = 0;
		TWord m_lastOpSize = 0;
		TWord m_singleOpWord = 0;
		TWord m_encodedInstructionCount = 0;
		TWord m_executedInstructionCount = 0;

		TWord m_nextPC = g_pcInvalid;
		TWord m_pMemWriteAddress = g_pcInvalid;
		TWord m_pMemWriteValue = 0;
		std::string m_dspAsm;
		bool m_possibleBranch = false;
		uint32_t m_flags = 0;
	};
}
