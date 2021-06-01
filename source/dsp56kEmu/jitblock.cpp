#include "jitblock.h"
#include "jitops.h"
#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	constexpr bool g_useSRCache = false;

	bool JitBlock::emit(const TWord _pc)
	{
		m_pcFirst = _pc;
		m_pMemSize = 0;
		m_dspAsm.clear();

		while(true)
		{
			const auto pc = m_pcFirst + m_pMemSize;

			// always terminate block if loop end has reached
			if(pc == static_cast<TWord>(m_dsp.regs().la.var + 1))
				break;

			JitOps ops(*this, g_useSRCache);

			std::string disasm;

			{
				TWord opA;
				TWord opB;
				m_dsp.memory().getOpcode(pc, opA, opB);
				m_dsp.disassembler().disassemble(disasm, opA, opB, 0, 0, 0);
				m_dspAsm += disasm + '\n';
			}

			ops.emit(pc);

			m_pMemSize += ops.getOpSize();
			++m_instructionCount;

			const auto res = ops.getInstruction();
			assert(res != InstructionCount);

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
		}

		m_dspRegs.clear();

		return !empty();
	}

	void JitBlock::exec()
	{
		m_nextPC = g_pcInvalid;
		m_func();
	}
}
