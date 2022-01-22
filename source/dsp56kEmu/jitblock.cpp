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

	JitBlock::~JitBlock()
	{
		assert(m_generating == false);
	}

	bool JitBlock::emit(Jit* _jit, const TWord _pc, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP)
	{
		JitBlockGenerating generating(*this);

		const bool isFastInterrupt = _pc < Vba_End;

		const TWord pcMax = isFastInterrupt ? (_pc + 2) : m_dsp.memory().size();

		m_pcFirst = _pc;
		m_pMemSize = 0;
		m_dspAsm.clear();
		bool shouldEmit = true;

		asmjit::BaseNode* cursorInsertPc = nullptr;
		asmjit::BaseNode* cursorEndInsertPc = nullptr;
		asmjit::BaseNode* cursorInsertEncodedInstructionCount = nullptr;

		// needed so that the dsp register is available
		dspRegPool().makeDspPtr(&m_dsp.getInstructionCounter(), sizeof(TWord));

		if(!isFastInterrupt)
		{
			// TODO: remove the whole block is the function statically jumps to m_child
			// This code is only used to set the value of the next PC to the default value (next instruction following this block)
			// Might be overwritten by a branching instruction
			cursorInsertPc = m_asm.cursor();						// inserted later below:	m_asm.mov(temp, asmjit::Imm(m_pcLast)); once m_pcLast is known
			m_dspRegPool.movDspReg(m_dsp.regs().pc, regReturnVal);
			cursorEndInsertPc = m_asm.cursor();
		}

		cursorInsertEncodedInstructionCount = m_asm.cursor();	// inserted later below:	m_mem.mov(temp, getEncodedInstructionCount());

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

				if(_jit && oi.flag(OpFlagBranch))
				{
					// if the last instruction of a JIT block is a branch to an address known at compile time, and this branch is fixed, i.e. is not dependant
					// on a condition: Store that address to be able to call the next JIT block from the current block without having to have a transition to the C++ code
					TWord opA;
					TWord opB;
					m_dsp.memory().getOpcode(pc, opA, opB);
					const auto branchTarget = getBranchTarget(oi.getInstruction(), opA, opB, pc);
					if(branchTarget != g_invalidAddress && branchTarget != g_dynamicAddress && branchTarget != m_pcFirst && _volatileP.find(branchTarget) == _volatileP.end())
					{
						assert(branchTarget < m_dsp.memory().size());

						m_childIsDynamic = hasField(oi.getInstruction(), Field_CCCC) || hasField(oi.getInstruction(), Field_bbbbb);

						const auto pcLast = m_pcFirst + m_pMemSize;

						// do not branch into ourself
						if (branchTarget < m_pcFirst || branchTarget >= pcLast)
						{
							auto* child = _jit->getChildBlock(this, branchTarget);

							if (child)
							{
								child->addParent(m_pcFirst);
								m_child = branchTarget;

								if(m_childIsDynamic)
								{
									auto* nonBranchChild = _jit->getChildBlock(this, pcLast);
									if (nonBranchChild)
									{
										nonBranchChild->addParent(m_pcFirst);
										m_nonBranchChild = pcLast;
									}
								}
							}
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

		auto pcNext = m_pcFirst + m_pMemSize;

		m_asm.setCursor(cursorInsertEncodedInstructionCount);
		increaseInstructionCount(asmjit::Imm(getEncodedInstructionCount()));
		m_asm.setCursor(m_asm.lastNode());

		if(cursorInsertPc)
		{
			if(m_child != g_invalidAddress && !m_childIsDynamic)
			{
				// remove the initial PC update completely, we know that we'll definitely branch
				int foo = 0;
				m_asm.removeNodes(cursorInsertPc->next(), cursorEndInsertPc);
			}
			else
			{
				// Inject the next PC into the setter at the start of this block
				m_asm.setCursor(cursorInsertPc);
				m_asm.mov(regReturnVal, asmjit::Imm(pcNext));
				m_asm.setCursor(m_asm.lastNode());
			}
		}

		if(appendLoopCode)
		{
			const auto skip = m_asm.newLabel();
			const auto enddo = m_asm.newLabel();

			JitOps ops(*this, isFastInterrupt);

			// It is important that this code does not allocate any temp registers inside of the branches. thefore, we prewarm everything
			RegGP temp(*this);

			const auto& sr = r32(m_dspRegPool.get(JitDspRegPool::DspSR, true, true));
			const auto& la = r32(m_dspRegPool.get(JitDspRegPool::DspLA, true, true));	// we don't use it here but do_end does
			const auto& lc = r32(m_dspRegPool.get(JitDspRegPool::DspLC, true, true));

			m_dspRegPool.lock(JitDspRegPool::DspSR);
			m_dspRegPool.lock(JitDspRegPool::DspLA);
			m_dspRegPool.lock(JitDspRegPool::DspLC);

			// check loop flag
#ifdef HAVE_ARM64
			m_asm.bitTest(sr, SRB_LF);
			m_asm.jz(skip);
#else
			m_asm.bt(sr, asmjit::Imm(SRB_LF));
			m_asm.jnc(skip);
#endif
			m_asm.cmp(lc, asmjit::Imm(1));
			m_asm.jle(enddo);
			m_asm.dec(lc);
			{
				const auto& ss = temp;
				m_dspRegs.getSS(ss);	// note: not calling getSSH as it will dec the SP
				m_asm.shr(ss, asmjit::Imm(24));
				m_asm.and_(ss, asmjit::Imm(0xffffff));
				setNextPC(ss);
			}
			m_asm.jmp(skip);

			m_asm.bind(enddo);
			ops.do_end(temp);

			m_asm.bind(skip);

			m_dspRegPool.unlock(JitDspRegPool::DspSR);
			m_dspRegPool.unlock(JitDspRegPool::DspLA);
			m_dspRegPool.unlock(JitDspRegPool::DspLC);
		}

		if(m_dspRegs.ccrDirtyFlags())
		{
			JitOps op(*this, isFastInterrupt);
			op.updateDirtyCCR();
		}

		m_dspRegPool.releaseAll();

		if (opFlags & JitOps::WritePMem)
			m_flags |= WritePMem;
		if (appendLoopCode)
			m_flags |= LoopEnd;

		const auto canBranch = (opFlags & WritePMem) == 0 && _jit && m_child != g_invalidAddress && _jit->canBeDefaultExecuted(m_child);
		if(canBranch && m_childIsDynamic)
			m_dspRegPool.movDspReg(regReturnVal, m_dsp.regs().pc);

		m_stack.popAll();

		if(empty())
			return false;

		if(canBranch)
		{
			const auto* child = _jit->getChildBlock(nullptr, m_child);

			assert(child->getFunc());

			if(m_childIsDynamic)
			{
				// we need to check if the PC has been set to the target address
				asmjit::Label skip = m_asm.newLabel();
				asmjit::Label end = m_asm.newLabel();

#ifdef HAVE_ARM64
				m_asm.mov(r32(g_funcArgGPs[1]), asmjit::Imm(m_child));
				m_asm.cmp(r32(regReturnVal), r32(g_funcArgGPs[1]));
#else
				m_asm.cmp(r32(regReturnVal), asmjit::Imm(m_child));
#endif

				m_asm.jnz(skip);
				m_stack.call(asmjit::func_as_ptr(child->getFunc()));

				if (m_nonBranchChild != g_invalidAddress)
					m_asm.jmp(end);

				m_asm.bind(skip);

				if(m_nonBranchChild != g_invalidAddress)
				{
					const auto nonBranchChild = _jit->getChildBlock(nullptr, m_nonBranchChild);
					m_stack.call(asmjit::func_as_ptr(nonBranchChild->getFunc()));
				}
				m_asm.bind(end);
			}
			else
			{
				m_stack.call(asmjit::func_as_ptr(child->getFunc()));
			}
		}
		else if (!m_possibleBranch && !isFastInterrupt && _jit && _cache[pcNext].block && !opFlags && !m_flags && m_child == g_invalidAddress)
		{
			auto* child = _jit->getChildBlock(this, pcNext, false);
			if(child)
			{
				child->addParent(m_pcFirst);
				m_stack.call(asmjit::func_as_ptr(child->getFunc()));
			}
		}

		return true;
	}

	void JitBlock::setNextPC(const JitRegGP& _pc)
	{
		m_dspRegPool.movDspReg(m_dsp.regs().pc, _pc);
		m_possibleBranch = true;
	}

	void JitBlock::increaseInstructionCount(const asmjit::Operand& _count)
	{
		const auto ptr = dspRegPool().makeDspPtr(&m_dsp.getInstructionCounter(), sizeof(TWord));

#ifdef HAVE_ARM64
		const auto r = r32(regReturnVal);
		m_asm.ldr(r, ptr);
		if (_count.isImm())
			m_asm.add(r, r, _count.as<asmjit::Imm>());
		else
			m_asm.add(r, r, _count.as<JitRegGP>());
		m_asm.str(r, ptr);
#else
		if(_count.isImm())
		{
			m_asm.add(ptr, _count.as<asmjit::Imm>());
		}
		else
		{
			m_asm.add(ptr, _count.as<JitRegGP>());
		}
#endif
	}

	void JitBlock::addParent(TWord _pc)
	{
		m_parents.insert(_pc);
	}
}
