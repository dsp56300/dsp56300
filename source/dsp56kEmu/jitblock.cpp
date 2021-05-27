#include "jitblock.h"
#include "jitops.h"

namespace dsp56k
{
	constexpr bool g_useSRCache = false;

	bool JitBlock::emit(TWord _pc)
	{
		m_pcFirst = _pc;
		m_pMemSize = 0;

		while(true)
		{
			const auto res = emitOne(m_pcFirst + m_pMemSize);
			if(res != Parallel)
			{
				const auto& oi = g_opcodes[res];

				if(oi.flag(OpFlagBranch))
					break;

				if(oi.flag(OpFlagLoop))
				{
					assert(false && "loop");
					break;
				}
				// TODO: P memory writes
			}
		}

		m_dspRegs.clear();

		return !empty();
	}

	Instruction JitBlock::emitOne(const TWord _pc)
	{
		JitOps ops(*this, g_useSRCache);

		ops.emit(_pc);

		m_pMemSize += ops.getOpSize();
		++m_instructionCount;

		const auto inst = ops.getInstruction();
		assert(inst != InstructionCount);
		return inst;
	}
}
