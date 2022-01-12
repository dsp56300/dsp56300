#include "dsp.h"
#include "interrupts.h"
#include "jitemitter.h"
#include "jitblock.h"
#include "jitops.h"
#include "memory.h"

namespace dsp56k
{
	constexpr uint32_t g_maxInstructionsPerBlock = 0;	// set to 1 for debugging/tracing

	JitBlock::JitBlock(JitEmitter& _a, DSP& _dsp, JitRuntimeData& _runtimeData)
	: m_runtimeData(_runtimeData)
	, m_asm(_a)
	, m_dsp(_dsp)
	, m_stack(*this)
	, m_xmmPool({regXMMTempA})
	, m_gpPool(g_regGPTemps)
	, m_dspRegs(*this)
	, m_dspRegPool(*this)
	, m_mem(*this)
	{
	}

	bool JitBlock::emit(const TWord _pc, std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP)
	{
		const bool isFastInterrupt = _pc < Vba_End;

		const TWord pcMax = isFastInterrupt ? (_pc + 2) : m_dsp.memory().size();

		m_pcFirst = _pc;
		m_pMemSize = 0;
		m_dspAsm.clear();
		bool shouldEmit = true;

		asmjit::BaseNode* cursorBeforePCUpdate = nullptr;
		asmjit::BaseNode* cursorAfterPCUpdate = nullptr;

		if(!isFastInterrupt)
		{
			// This code is only used to set the value of the next PC to the default value. Might be overwritten by a branch.
			// If this code does not have a branch (only known after generation has finished), this code block is removed

			const RegGP temp(*this);
			cursorBeforePCUpdate = m_asm.cursor();
			m_mem.mov(temp, m_pcLast);
			m_mem.mov(nextPC(), temp);
			cursorAfterPCUpdate = m_asm.cursor();
		}

		{
			const RegGP temp(*this);
			m_mem.mov(temp, getEncodedInstructionCount());
			m_mem.mov(getExecutedInstructionCount(), temp.get());
		}

		uint32_t opFlags = 0;
		bool appendLoopCode = false;

		while(shouldEmit)
		{
			const auto pc = m_pcFirst + m_pMemSize;

			if(pc >= pcMax)
				break;

			// do never overwrite code that already exists
			if(_cache[pc].block)
				break;

			// for a volatile P address, if you have some code, break now. if not, generate this one op, and then return.
			if (_volatileP.find(pc) != _volatileP.end())
			{
				if (m_encodedInstructionCount) 
					break;
				shouldEmit = false;
			}

			JitOps ops(*this, isFastInterrupt);

#if defined(_MSC_VER) && defined(_DEBUG)
			{
				std::string disasm;
				TWord opA;
				TWord opB;
				m_dsp.memory().getOpcode(pc, opA, opB);
				m_dsp.disassembler().disassemble(disasm, opA, opB, 0, 0, 0);
//				LOG(HEX(pc) << ": " << disasm);
				m_dspAsm += disasm + '\n';
			}
#endif
			m_asm.nop();
			ops.emit(pc);
			m_asm.nop();

			opFlags |= ops.getResultFlags();
			
			m_singleOpWord = ops.getOpWordA();
			
			m_pMemSize += ops.getOpSize();
			++m_encodedInstructionCount;

			const auto res = ops.getInstruction();
			assert(res != InstructionCount);

			m_lastOpSize = ops.getOpSize();

			// always terminate block if loop end has reached
			if((m_pcFirst + m_pMemSize) == static_cast<TWord>(m_dsp.regs().la.var + 1))
			{
				appendLoopCode = true;
				break;
			}
			
			if(ops.checkResultFlag(JitOps::WritePMem) || ops.checkResultFlag(JitOps::WriteToLA) || ops.checkResultFlag(JitOps::WriteToLC))
				break;

			if(res != Parallel)
			{
				const auto& oi = g_opcodes[res];

				if(oi.flag(OpFlagBranch))
				{
					// if the last instruction of a JIT block is a branch to an address known at compile time, and this branch is fixed, i.e. is not dependant
					// on a condition: Store that address to be able to call the next JIT block from the current block without having to have a transition to the C++ code
					TWord opA;
					TWord opB;
					m_dsp.memory().getOpcode(pc, opA, opB);
					const auto branchTarget = getBranchTarget(oi.getInstruction(), opA, opB, pc);
					if(branchTarget != g_invalidAddress && branchTarget != g_dynamicAddress && branchTarget != m_pcFirst)
					{
						assert(branchTarget < m_dsp.memory().size());
						if(!hasField(oi.getInstruction(), Field_CCCC) && !hasField(oi.getInstruction(), Field_bbbbb))
						{
							m_child = branchTarget;
						}
					}
					break;
				}

				if (oi.flag(OpFlagPopPC))
					break;

				if(oi.flag(OpFlagLoop))
					break;
			}

			if(g_maxInstructionsPerBlock > 0 && m_encodedInstructionCount >= g_maxInstructionsPerBlock)
			{
				// we can NOT prematurely interrupt the creation of a fast interrupt block
				if(!isFastInterrupt)
				{
					m_flags |= InstructionLimit;
					break;
				}
			}
		}

		m_pcLast = m_pcFirst + m_pMemSize;

#if 0
		if(appendLoopCode)	// this block works, but I'm not sure if we want to keep it here as it increases code size, while the code in jit.cpp does the same but exists only once
		{
			const auto end = m_asm.newLabel();
			const auto decLC = m_asm.newLabel();

			dspRegPool().releaseAll();
			stack().pushNonVolatiles();

			JitOps ops(*this, isFastInterrupt);
//			DSPReg lc(*this, JitDspRegPool::DspLC, true, true);
			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_LF));	// check loop flag
			m_asm.jnc(end);
			m_asm.cmp(m_dspRegs.getLC(JitDspRegs::Read), asmjit::Imm(1));
			dspRegPool().releaseAll();
			m_asm.jg(decLC);
			ops.do_end();
			dspRegPool().releaseAll();
			m_asm.jmp(end);

			m_asm.bind(decLC);
			m_asm.dec(m_dspRegs.getLC(JitDspRegs::ReadWrite));
			{
				const RegGP ss(*this);
				m_dspRegs.getSS(ss);
				m_asm.shr(ss, asmjit::Imm(24));
				m_asm.and_(ss, asmjit::Imm(0xffffff));
				setNextPC(ss);
			}

			dspRegPool().releaseAll();
			m_asm.bind(end);
		}
#endif
		if(m_possibleBranch)
		{
			mem().mov(m_dspRegPool.get(JitDspRegPool::DspPC, false, true), nextPC());
		}
		else if(!isFastInterrupt)
		{
			if (cursorBeforePCUpdate && cursorAfterPCUpdate)
				m_asm.removeNodes(cursorBeforePCUpdate->next(), cursorAfterPCUpdate);

			m_asm.mov(m_dspRegPool.get(JitDspRegPool::DspPC, false, true), asmjit::Imm(m_pcLast));
		}

		if(m_dspRegs.ccrDirtyFlags())
		{
			JitOps op(*this, isFastInterrupt);
			op.updateDirtyCCR();
		}
		m_dspRegPool.releaseAll();

		m_stack.popAll();

		if(empty())
			return false;
		if(opFlags & JitOps::WritePMem)
			m_flags |= WritePMem;
		if(appendLoopCode)
			m_flags |= LoopEnd;
		return true;
	}

	void JitBlock::setNextPC(const JitRegGP& _pc)
	{
		mem().mov(nextPC(), _pc);
		m_possibleBranch = true;
	}
}
