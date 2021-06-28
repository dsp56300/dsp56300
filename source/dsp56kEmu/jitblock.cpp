#include "jitblock.h"
#include "jitops.h"
#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	constexpr uint32_t g_maxInstructionsPerBlock = 0;	// set to 1 for debugging/tracing

	JitBlock::JitBlock(asmjit::x86::Assembler& _a, DSP& _dsp)
	: m_asm(_a)
	, m_dsp(_dsp)
	, m_stack(*this)
	, m_xmmPool({regXMMTempA, regXMMTempB, regXMMTempC})
	, m_gpPool({regGPTempA, regGPTempB, regGPTempC, regGPTempD, regGPTempE})
	, m_dspRegs(*this)
	, m_dspRegPool(*this)
	, m_mem(*this)
	{
	}

	bool JitBlock::emit(const TWord _pc, std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP)
	{
		m_pcFirst = _pc;
		m_pMemSize = 0;
		m_dspAsm.clear();
		bool shouldEmit = true;

		while(shouldEmit)
		{
			const auto pc = m_pcFirst + m_pMemSize;

			// do never overwrite code that already exists
			if(_cache[pc].block)
				break;

			// always terminate block if loop end has reached
			if(m_encodedInstructionCount && pc == static_cast<TWord>(m_dsp.regs().la.var + 1))
				break;
			
			// for a volatile P address, if you have some code, break now. if not, generate this one op, and then return.
			if (_volatileP.find(pc)!=_volatileP.end())
			{
				if (m_encodedInstructionCount) break;
				else shouldEmit=false;
			}

			JitOps ops(*this);

			if(false)
			{
				std::string disasm;
				TWord opA;
				TWord opB;
				m_dsp.memory().getOpcode(pc, opA, opB);
				m_dsp.disassembler().disassemble(disasm, opA, opB, 0, 0, 0);
//				LOG(HEX(pc) << ": " << disasm);
				m_dspAsm += disasm + '\n';
			}

			m_asm.nop();
			ops.emit(pc);
			m_asm.nop();

			m_pMemSize += ops.getOpSize();
			++m_encodedInstructionCount;

			const auto res = ops.getInstruction();
			assert(res != InstructionCount);

			m_lastOpSize = ops.getOpSize();

			if(ops.checkResultFlag(JitOps::WritePMem) || ops.checkResultFlag(JitOps::WriteToLA) || ops.checkResultFlag(JitOps::WriteToLC))
				break;

			if(res != Parallel)
			{
				const auto& oi = g_opcodes[res];

				if(oi.flag(OpFlagBranch))
					break;

				if(oi.flag(OpFlagLoop))
					break;

				if(oi.flag(OpFlagPopPC))
					break;
			}

			if(g_maxInstructionsPerBlock > 0 && m_encodedInstructionCount >= g_maxInstructionsPerBlock)
				break;
		}

		if(m_dspRegs.ccrDirtyFlags())
		{
			JitOps op(*this);
			op.updateDirtyCCR();
		}
		m_dspRegPool.releaseAll();

		m_stack.popAll();

		return !empty();
	}

	void JitBlock::exec()
	{
		m_nextPC = m_pcFirst + getPMemSize();
		m_executedInstructionCount = m_encodedInstructionCount;
		m_pMemWriteAddress = g_pcInvalid;
		m_func();

		if(m_dspRegs.hasDirtyMRegisters())
			m_dspRegs.updateDspMRegisters();
	}
}
