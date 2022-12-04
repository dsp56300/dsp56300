#include "dsp.h"
#include "interrupts.h"
#include "jitemitter.h"
#include "jitblock.h"

#include "jitblockinfo.h"
#include "jitops.h"
#include "memory.h"

namespace dsp56k
{
	JitBlock::JitBlock(JitEmitter& _a, DSP& _dsp, JitRuntimeData& _runtimeData, const JitConfig& _config)
	: m_runtimeData(_runtimeData)
	, m_asm(_a)
	, m_dsp(_dsp)
	, m_stack(*this)
	, m_xmmPool({regXMMTempA})
	, m_gpPool(g_regGPTemps)
	, m_dspRegs(*this)
	, m_dspRegPool(*this)
	, m_mem(*this)
	, m_config(_config)
	{
	}

	JitBlock::~JitBlock()
	{
		assert(m_generating == false);
	}

	void JitBlock::getInfo(JitBlockInfo& _info, const DSP& _dsp, const TWord _pc, const JitConfig& _config, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds)
	{
		const auto& opcodes = _dsp.opcodes();

		const bool isFastInterrupt = _pc < Vba_End;

		const TWord pcMax = isFastInterrupt ? (_pc + 2) : _dsp.memory().sizeP();

		bool shouldEmit = true;

		auto& readRegs = _info.readRegs;
		auto& writtenRegs = _info.writtenRegs;

		bool writesSR = false;
		bool readsSR = false;

		auto& numWords = _info.memSize;
		auto& numInstructions = _info.instructionCount;

		auto& terminationReason = _info.terminationReason;

		_info.pc = _pc;

		if(_loopStarts.find(_pc - 2) != _loopStarts.end())
		{
			assert(_pc == hiword(_dsp.regs().ss[_dsp.ssIndex()]).toWord());
			_info.addFlag(JitBlockInfo::Flags::IsLoopBodyBegin);
		}
		else
		{
			assert(_pc != hiword(_dsp.regs().ss[_dsp.ssIndex()]).toWord());
		}

		auto writesM = RegisterMask::None;
		auto readsM = RegisterMask::None;

		while(shouldEmit)
		{
			const auto pc = _pc + numWords;

			if(pc >= pcMax)
			{
				terminationReason = JitBlockInfo::TerminationReason::PcMax;
				break;
			}

			// never overwrite code that already exists
			if(_cache[pc].block)
			{
				terminationReason = JitBlockInfo::TerminationReason::ExistingCode;
				break;
			}

			TWord opA;
			TWord opB;
			_dsp.memory().getOpcode(pc, opA, opB);

			Instruction instA, instB;

			opcodes.getInstructionTypes(opA, instA, instB);

			auto written = RegisterMask::None;
			auto read = RegisterMask::None;

			opcodes.getRegisters(written, read, opA, opB);

			const auto writtenM = (written & RegisterMask::M);
			const auto readM = read & RegisterMask::M;

			const auto flags = Opcodes::getFlags(instA, instB);

			// a jsr in a fast interrupt modifies the MR because it disables scaling mode bits, loop flag and sixteen-bit arithmetic mode
			if(isFastInterrupt && (written & RegisterMask::SSL) != RegisterMask::None)
				written |= RegisterMask::MR;

			const auto srModeChange = (written & (RegisterMask::EMR | RegisterMask::MR)) != RegisterMask::None;

			if(srModeChange)
			{
				_info.addFlag(JitBlockInfo::Flags::ModeChange);
			}

			if(writtenM != RegisterMask::None)
			{
				writesM |= writtenM;
				_info.addFlag(JitBlockInfo::Flags::ModeChange);
			}
			if(readM != RegisterMask::None)
			{
				const auto readAfterWrite = readM & writesM;
				readsM |= readAfterWrite;
			}

			// while an M register change causes a mode change, we can continue this block as long as that M register is not read in a subsequent instruction
			if((writesM & readsM) != RegisterMask::None)
			{
				if(numInstructions)
				{
					_info.terminationReason = JitBlockInfo::TerminationReason::ModeChange;
					break;
				}
			}

			// for a volatile P address, if you have some code, break now. if not, generate this one op, and then return.
			if (_volatileP.find(pc) != _volatileP.end() || 
				(_volatileP.find(pc+1) != _volatileP.end() && opcodes.getOpcodeLength(opA, instA, instB) == 2))
			{
				terminationReason = JitBlockInfo::TerminationReason::VolatileP;
				if (numInstructions)
					break;
				shouldEmit = false;
			}

			const auto isRep = flags & (OpFlagRepDynamic | OpFlagRepImmediate);

			writtenRegs |= written;
			readRegs |= read;

			if ((readRegs & RegisterMask::SR) != RegisterMask::None)
				readsSR = true;

			if ((writtenRegs & RegisterMask::SR) != RegisterMask::None)
				writesSR = true;

			if (writesSR && !readsSR)
				_info.addFlag(JitBlockInfo::Flags::WritesSRbeforeRead);

			numWords += opcodes.getOpcodeLength(opA);
			++numInstructions;

			if(getLoopEndAddr(_info.loopEnd, instA, pc, opB))
				_info.loopBegin = pc;

			if(!isRep)
			{
				if(flags & OpFlagBranch)
				{
					terminationReason = JitBlockInfo::TerminationReason::Branch;
					_info.branchTarget = getBranchTarget(instA, opA, opB, pc);
					_info.branchIsConditional = hasField(instA, Field_CCCC) || hasField(instA, Field_bbbbb);
					break;
				}
				if(flags & OpFlagPopPC)
				{
					terminationReason = JitBlockInfo::TerminationReason::PopPC;
					break;
				}
				if(any(written, RegisterMask::LA | RegisterMask::LC))
				{
					terminationReason = JitBlockInfo::TerminationReason::WriteLoopRegs;
					break;
				}

				if(flags & OpFlagLoop)
				{
					terminationReason = JitBlockInfo::TerminationReason::LoopBegin;
					break;
				}
			}

			// always terminate block if loop end has reached
			if(_loopEnds.find(_pc + numWords) != _loopEnds.end())
			{
				assert((_pc + numWords) == static_cast<TWord>(_dsp.regs().la.var + 1));
				terminationReason = JitBlockInfo::TerminationReason::LoopEnd;
				break;
			}

			if(opcodes.writesToPMemory(opA))
			{
				terminationReason = JitBlockInfo::TerminationReason::WritePMem;
				break;
			}

			if(srModeChange)
			{
				terminationReason = JitBlockInfo::TerminationReason::ModeChange;
				break;
			}

			// we can NOT prematurely interrupt the creation of a fast interrupt block
			if(!isRep && !isFastInterrupt && _config.maxInstructionsPerBlock > 0 && numInstructions >= _config.maxInstructionsPerBlock)
			{
				terminationReason = JitBlockInfo::TerminationReason::InstructionLimit;
				break;
			}
		}
	}

	bool JitBlock::emit(JitBlockChain* _chain, const TWord _pc, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds, bool _profilingSupport)
	{
		JitBlockGenerating generating(*this);

		m_chain = _chain;

		const bool isFastInterrupt = _pc < Vba_End;

		m_pcFirst = _pc;
		m_pMemSize = 0;
		m_dspAsm.clear();

		auto loopBegin = m_asm.newNamedLabel("loopBegin");
		m_asm.bind(loopBegin);

		asmjit::BaseNode* cursorInsertEncodedInstructionCount = nullptr;

		// needed so that the dsp register is available
		dspRegPool().makeDspPtr(&m_dsp.getInstructionCounter(), sizeof(TWord));

		cursorInsertEncodedInstructionCount = m_asm.cursor();	// inserted later below:	m_mem.mov(temp, getEncodedInstructionCount());

		uint32_t blockFlags = 0;

		getInfo(m_info, dsp(), _pc, m_config, _cache, _volatileP, _loopStarts, _loopEnds);

		const auto pcNext = _pc + m_info.memSize;

		if(!isFastInterrupt && m_info.terminationReason != JitBlockInfo::TerminationReason::PopPC)
		{
			if(m_info.branchTarget == g_invalidAddress || m_info.branchIsConditional)
			{
				DspValue pc(*this, pcNext, DspValue::Immediate24);
				m_dspRegPool.write(JitDspRegPool::DspPC, pc);
			}
		}

		TWord opA = 0;
		TWord opB = 0;

#if defined(_MSC_VER) && defined(_DEBUG)
		std::string opDisasm;
#endif

		uint32_t opPC = 0;

		while(m_pMemSize < m_info.memSize)
		{
			opPC = _pc + m_pMemSize;

			m_dsp.memory().getOpcode(opPC, opA, opB);

			JitOps ops(*this, isFastInterrupt);

#if defined(_MSC_VER) && defined(_DEBUG)
			m_dsp.disassembler().disassemble(opDisasm, opA, opB, 0, 0, 0);
//			LOG(HEX(pc) << ": " << opDisasm);

			{
				Instruction instA, instB;
				m_dsp.opcodes().getInstructionTypes(opA, instA, instB);
				const auto& oi = g_opcodes[instA];

				if(oi.flag(OpFlagRepImmediate) || oi.flag(OpFlagRepDynamic))
				{
					std::string repInst;
					m_dsp.disassembler().disassemble(repInst, opB, 0, 0, 0, 0);
					opDisasm += '\n' + repInst;
//					LOG("REP:" << opDisasm);
				}
			}

			m_dspAsm += opDisasm + '\n';
#endif

			if(_profilingSupport)
			{
				const auto labelBegin = m_asm.newLabel();
				const auto labelEnd = m_asm.newLabel();

				m_asm.bind(labelBegin);

				const InstructionProfilingInfo pi{ opPC, opA, opB, 0, 1, labelBegin, labelEnd, 0, 0, std::string()};
				m_profilingInfo.emplace_back(pi);
			}

			if(m_config.splitOpsByNops)
				m_asm.nop();
			ops.emit(opPC, opA, opB);
			if (m_config.splitOpsByNops)
				m_asm.nop();

			if (_profilingSupport)
			{
				auto& pi = m_profilingInfo.back();

				pi.opLen = ops.getOpSize();

				const auto& oi = g_opcodes[ops.getInstruction()];

				if(oi.flag(OpFlagRepImmediate) || oi.flag(OpFlagRepDynamic))
					pi.lineCount++;

				m_asm.bind(pi.labelAfter);
			}

			blockFlags |= ops.getResultFlags();
			
			m_singleOpWordA = opA;
			m_singleOpWordB = opB;

			m_pMemSize += ops.getOpSize();
			++m_encodedInstructionCount;

			m_lastOpSize = ops.getOpSize();
		}

		auto canBranch = m_info.terminationReason != JitBlockInfo::TerminationReason::WritePMem && !m_info.hasFlag(JitBlockInfo::Flags::ModeChange);

		bool childIsConditional = false;

		if(_chain && m_info.terminationReason == JitBlockInfo::TerminationReason::Branch)
		{
			// if the last instruction of a JIT block is a branch to an address known at compile time, and this branch is fixed, i.e. is not dependant
			// on a condition: Store that address to be able to call the next JIT block from the current block without having to have a transition to the C++ code

			const auto branchTarget = m_info.branchTarget;

			if (branchTarget != g_invalidAddress)
			{
				assert(branchTarget == g_dynamicAddress || branchTarget < m_dsp.memory().sizeP());

				const auto pcLast = m_pcFirst + m_pMemSize;

				childIsConditional = m_info.branchIsConditional;

				if (branchTarget == g_dynamicAddress)
				{
					m_child = g_dynamicAddress;
				}
				// do not branch into ourself
				else if (branchTarget < m_pcFirst || branchTarget >= pcLast)
				{
					auto* child = _chain->getChildBlock(this, branchTarget);

					if (child)
					{
						child->addParent(m_pcFirst);
						m_child = branchTarget;

						if (childIsConditional)
						{
							auto* nonBranchChild = _chain->getChildBlock(this, pcLast);
							if (nonBranchChild && _chain->canBeDefaultExecuted(pcLast))
							{
								nonBranchChild->addParent(m_pcFirst);
								m_nonBranchChild = pcLast;
							}
						}
					}
				}
			}
		}

		if (m_info.terminationReason == JitBlockInfo::TerminationReason::PopPC)
			blockFlags |= JitOps::PopPC;

		m_asm.setCursor(cursorInsertEncodedInstructionCount);
		increaseInstructionCount(asmjit::Imm(getEncodedInstructionCount()));
		m_asm.setCursor(m_asm.lastNode());

		const auto isLoopStart = m_info.hasFlag(JitBlockInfo::Flags::IsLoopBodyBegin);
		const auto isLoopEnd = m_info.terminationReason == JitBlockInfo::TerminationReason::LoopEnd;
		const auto isLoopBody = isLoopStart && isLoopEnd;

		auto jumpIfLoop = [&](const asmjit::Label& _ifTrue, const JitRegGP& _compare)
		{
			if (isLoopBody)
			{
#ifdef HAVE_ARM64
				RegGP temp(*this);
				m_asm.mov(r32(temp), asmjit::Imm(m_pcFirst));
				m_asm.cmp(r32(_compare), r32(temp));
#else
				m_asm.cmp(r32(_compare), asmjit::Imm(m_pcFirst));
#endif
				m_asm.jz(_ifTrue);
				return true;
			}
			return false;
		};

		auto profileBegin = [&](const std::string& _name)
		{
			if(!_profilingSupport)
				return asmjit::Label();

			const auto labelBegin = m_asm.newLabel();
			const auto labelEnd = m_asm.newLabel();

			m_asm.bind(labelBegin);

			const InstructionProfilingInfo pi{ g_invalidAddress, 0, 0, 0, 1, labelBegin, labelEnd, 0, 0, _name};
			m_profilingInfo.emplace_back(pi);

			return labelEnd;
		};

		auto profileEnd = [&](const asmjit::Label& l)
		{
			if(l.isValid())
				m_asm.bind(l);
		};

		if(isLoopEnd)
		{
			auto pl = profileBegin("loop");

			const auto skip = m_asm.newLabel();
			const auto enddo = m_asm.newLabel();

			JitOps ops(*this, isFastInterrupt);

			// It is important that this code does not allocate any temp registers inside of the branches. thefore, we prewarm everything
			RegGP temp(*this);

			const auto& sr = r32(m_dspRegPool.get(JitDspRegPool::DspSR, true, true));
			                 r32(m_dspRegPool.get(JitDspRegPool::DspLA, true, true));	// we don't use it here but do_end does
			const auto& lc = r32(m_dspRegPool.get(JitDspRegPool::DspLC, true, true));

			m_dspRegPool.lock(JitDspRegPool::DspSR);
			m_dspRegPool.lock(JitDspRegPool::DspLA);
			m_dspRegPool.lock(JitDspRegPool::DspLC);

			DSPReg pc(*this, JitDspRegPool::DspPC, true, false);

			// check loop flag

			m_asm.bitTest(sr, SRB_LF);
			m_asm.jz(skip);

			m_asm.cmp(lc, asmjit::Imm(1));
			m_asm.jle(enddo);
			m_asm.dec(lc);
			{
				const auto& ss = temp;
				m_dspRegs.getSS(ss);	// note: not calling getSSH as it will dec the SP
#ifdef HAVE_ARM64
				m_asm.ubfx(ss, ss, asmjit::Imm(24), asmjit::Imm(24));
#else
				m_asm.shr(ss, asmjit::Imm(24));
#endif
				setNextPC(ss);
			}
			m_asm.jmp(skip);

			m_asm.bind(enddo);
			ops.do_end(temp);

			m_asm.bind(skip);

			m_dspRegPool.unlock(JitDspRegPool::DspSR);
			m_dspRegPool.unlock(JitDspRegPool::DspLA);
			m_dspRegPool.unlock(JitDspRegPool::DspLC);

			profileEnd(pl);
		}

		if(m_dspRegs.ccrDirtyFlags())
		{
			auto pl = profileBegin("ccrUpdate");

			asmjit::Label skipCCRupdate = m_asm.newLabel();

			// We skip to update dirty CCRs if we are running a loop and the loop has not yet ended.
			// Be sure to not skip CCR updates if the loop itself reads the SR before it has written to it
			const auto writesSRbeforeRead = m_info.hasFlag(JitBlockInfo::Flags::WritesSRbeforeRead);
			const auto readsSR = (m_info.readRegs & RegisterMask::SR) != RegisterMask::None;
			if(isLoopBody && (writesSRbeforeRead || !readsSR))
			{
				m_dspRegPool.read(regReturnVal, JitDspRegPool::DspPC);
				jumpIfLoop(skipCCRupdate, m_dspRegPool.get(JitDspRegPool::DspPC, true, false));
			}

			JitOps op(*this, isFastInterrupt);

			op.updateDirtyCCR();

			m_asm.bind(skipCCRupdate);

			profileEnd(pl);
		}

		auto pl = profileBegin("release");

		canBranch &= _chain && m_child != g_invalidAddress && m_child != g_dynamicAddress && _chain->canBeDefaultExecuted(m_child);

		if((canBranch && childIsConditional) || isLoopBody)
			m_dspRegPool.read(regReturnVal, JitDspRegPool::DspPC);

		m_dspRegPool.releaseAll();
		m_stack.popAll();

		if(empty())
		{
			profileEnd(pl);
			return false;
		}

		if (canBranch)
		{
			const auto* child = _chain->getChildBlock(nullptr, m_child);

			assert(child->getFunc());

			if (childIsConditional)
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

				JitBlock* nonBranchChild = nullptr;

				if (m_nonBranchChild != g_invalidAddress && _chain->canBeDefaultExecuted(m_nonBranchChild))
				{
					nonBranchChild = _chain->getChildBlock(nullptr, m_nonBranchChild);
					if(nonBranchChild)
						m_asm.jmp(end);
				}

				m_asm.bind(skip);

				if (nonBranchChild)
					m_stack.call(asmjit::func_as_ptr(nonBranchChild->getFunc()));

				m_asm.bind(end);
			}
			else
			{
				m_stack.call(asmjit::func_as_ptr(child->getFunc()));
			}
		}
		else if(!jumpIfLoop(loopBegin, regReturnVal))
		{
			if ((getInfo().terminationReason == JitBlockInfo::TerminationReason::ExistingCode || getInfo().terminationReason == JitBlockInfo::TerminationReason::VolatileP))
			{
				if(!isFastInterrupt && !blockFlags && !isLoopEnd && _chain && _chain->canBeDefaultExecuted(pcNext))
				{
					auto* child = _chain->getChildBlock(this, pcNext, false);
					if(child)
					{
						m_child = pcNext;
						child->addParent(m_pcFirst);
						m_stack.call(asmjit::func_as_ptr(child->getFunc()));
					}
				}
			}
		}

		profileEnd(pl);

		/*
		bool anyXmm = false;

		auto* node = m_asm.firstNode();
		while(node)
		{
			if(!node->isInst())
			{
				node = node->next();
				continue;
			}

			const auto inst = node->as<asmjit::InstNode>();
			asmjit::InstRWInfo rwInfo{};
			asmjit::InstAPI::queryRWInfo(asmjit::Arch::kX64, inst->baseInst(), inst->_opArray, inst->opCount(), &rwInfo);

			for(size_t i=0; i<rwInfo.opCount(); ++i)
			{
				const auto& op = inst->operands()[i];

				if(!op.isReg())
					continue;

				const auto& reg = op.as<asmjit::x86::Reg>();

				const asmjit::OpRWInfo& opInfo = rwInfo.operand(i);

				bool rw = opInfo.isReadWrite();
				bool written = opInfo.isWrite() || opInfo.isReadWrite();
				bool read = opInfo.isRead() || opInfo.isReadWrite();

				if(reg.isXmm())
				{
					const auto& x = reg.as<asmjit::x86::Xmm>();
					if(rw)
						LOG(i << " rw xmm" << x._baseId)
					else if(written)
						LOG(i << " write xmm" << x._baseId)
					else if(read)
						LOG(i << " read xmm" << x._baseId)
					anyXmm = true;
				}
			}

			node = node->next();
		}
		if(anyXmm)
			LOG("END");
		*/
		return true;
	}

	void JitBlock::finalize(const TJitFunc _func, const asmjit::CodeHolder& _codeHolder)
	{
		m_func = _func;
		m_codeSize = _codeHolder.codeSize();

		for (auto& pi : m_profilingInfo)
		{
			pi.codeOffset = _codeHolder.labelOffset(pi.labelBefore);
			pi.codeOffsetAfter = _codeHolder.labelOffset(pi.labelAfter);
		}
	}

	void JitBlock::setNextPC(const JitRegGP& _pc)
	{
		m_dspRegPool.write(JitDspRegPool::DspPC, _pc);
		m_possibleBranch = true;
	}

	void JitBlock::setNextPC(const DspValue& _pc)
	{
		m_dspRegPool.write(JitDspRegPool::DspPC, _pc);
		m_possibleBranch = true;
	}

	uint64_t JitBlock::getSingleOpCacheKey(const TWord _opA, const TWord _opB)
	{
		uint64_t res = _opA;
		res <<= 32;
		res |= _opB;
		return res;
	}

	void JitBlock::increaseInstructionCount(const asmjit::Operand& _count)
	{
		const auto ptr = dspRegPool().makeDspPtr(&m_dsp.getInstructionCounter(), sizeof(TWord));

#ifdef HAVE_ARM64
		const RegScratch scratch(*this);
		const auto r = r32(scratch);
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

	AddressingMode JitBlock::getAddressingMode(const uint32_t _aguIndex) const
	{
		return m_chain ? m_chain->getMode().getAddressingMode(_aguIndex) : AddressingMode::Unknown;
	}

	const JitDspMode* JitBlock::getMode() const
	{
		return m_chain ? &m_chain->getMode() : nullptr;
	}

	void JitBlock::addParent(const TWord _pc)
	{
		m_parents.insert(_pc);
	}
}
