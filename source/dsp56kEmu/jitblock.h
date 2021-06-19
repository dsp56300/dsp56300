#pragma once

#include "jitdspregs.h"
#include "jitmem.h"
#include "jitregtracker.h"
#include "jitregtypes.h"

#include <string>
#include <vector>

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

	class JitBlock
	{
	public:
		typedef void (*JitEntry)();

		JitBlock(asmjit::x86::Assembler& _a, DSP& _dsp)
		: m_asm(_a)
		, m_dsp(_dsp)
		, m_xmmPool({regXMMTempA, regXMMTempB, regXMMTempC})
		, m_gpPool({regGPTempA, regGPTempB, regGPTempC, regGPTempD, regGPTempE})
		, m_dspRegs(*this)
		, m_mem(*this)
		{
		}

		asmjit::x86::Assembler& asm_() { return m_asm; }
		DSP& dsp() { return m_dsp; }
		JitRegpool<JitReg64>& gpPool() { return m_gpPool; }
		JitRegpool<JitReg128>& xmmPool() { return m_xmmPool; }
		JitDspRegs& regs() { return m_dspRegs; }
		Jitmem& mem() { return m_mem; }

		operator JitRegpool<JitReg64>& ()		{ return m_gpPool; }
		operator JitRegpool<JitReg128>& ()		{ return m_xmmPool;	}
		operator asmjit::x86::Assembler& ()		{ return m_asm;	}

		bool emit(TWord _pc, std::vector<JitBlock*>& _cache);
		bool empty() const { return m_pMemSize == 0; }
		TWord getPCFirst() const { return m_pcFirst; }
		TWord getPMemSize() const { return m_pMemSize; }

		void setFunc(const JitEntry _func) { m_func = _func; }
		const JitEntry& getFunc() const { return m_func; }

		void exec();
		TWord getEncodedInstructionCount() const { return m_encodedInstructionCount; }
		const TWord& getExecutedInstructionCount() const { return m_executedInstructionCount; }

		// JIT code writes these
		TWord& nextPC() { return m_nextPC; }
		uint32_t& pMemWriteAddress() { return m_pMemWriteAddress; }
		uint32_t& pMemWriteValue() { return m_pMemWriteValue; }
		const std::string& getDisasm() const { return m_dspAsm; }

	private:
		JitEntry m_func = nullptr;

		asmjit::x86::Assembler& m_asm;
		DSP& m_dsp;
		JitRegpool<JitReg128> m_xmmPool;
		JitRegpool<JitReg64> m_gpPool;
		JitDspRegs m_dspRegs;
		Jitmem m_mem;

		TWord m_pcFirst = 0;
		TWord m_pMemSize = 0;
		TWord m_encodedInstructionCount = 0;
		TWord m_executedInstructionCount = 0;

		TWord m_nextPC = g_pcInvalid;
		TWord m_pMemWriteAddress = g_pcInvalid;
		TWord m_pMemWriteValue = 0;
		std::string m_dspAsm;
	};
}
