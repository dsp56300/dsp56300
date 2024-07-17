#include "dsp.h"
#include "interrupts.h"
#include "jitemitter.h"
#include "jitblock.h"

#include "jitblockinfo.h"
#include "jitblockruntimedata.h"
#include "jitops.h"
#include "memory.h"
#include "opcodecycles.h"

namespace dsp56k
{
	JitBlock::JitBlock(JitEmitter& _a, DSP& _dsp, JitRuntimeData& _runtimeData, JitConfig&& _config)
	: m_runtimeData(_runtimeData)
	, m_asm(_a)
	, m_dsp(_dsp)
	, m_stack(*this)
	, m_xmmPool({regXMMTempA})
	, m_gpPool(g_regGPTemps)
	, m_dspRegs(*this)
	, m_dspRegPool(*this)
	, m_mem(*this)
	, m_config(std::move(_config))
	{
	}

	JitBlock::~JitBlock() = default;

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
		auto& numCycles = _info.cycleCount;

		auto& terminationReason = _info.terminationReason;

		_info.pc = _pc;

		if(_loopStarts.find(_pc - 2) != _loopStarts.end())
		{
			assert(_pc == hiword(_dsp.regs().ss[_dsp.ssIndex()]).toWord());
			_info.addFlag(JitBlockInfo::Flags::IsLoopBodyBegin);
		}
		else
		{
			assert(_pc == 0 || _pc != hiword(_dsp.regs().ss[_dsp.ssIndex()]).toWord());
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
			if(pc < _cache.size() && _cache[pc].block)
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

			Opcodes::getRegisters(written, read, opA, instA, instB);

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
				(_volatileP.find(pc+1) != _volatileP.end() && Opcodes::getOpcodeLength(opA, instA, instB) == 2))
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

			numWords += Opcodes::getOpcodeLength(opA, instA, instB);
			++numInstructions;
			numCycles += calcCycles(instA, instB, pc, opA, _dsp.memory().getBridgedMemoryAddress(), 1);

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

				if(instA == Wait)
				{
					terminationReason = JitBlockInfo::TerminationReason::WaitInstruction;
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

			if(writesToPMemory(instA, opA) || writesToPMemory(instB, opA))
			{
				terminationReason = JitBlockInfo::TerminationReason::WritePMem;
				break;
			}

			if(srModeChange)
			{
				terminationReason = JitBlockInfo::TerminationReason::ModeChange;
				break;
			}

			// we must NOT prematurely abort the creation of a fast interrupt block
			if(!isRep && !isFastInterrupt && _config.maxInstructionsPerBlock > 0 && numInstructions >= _config.maxInstructionsPerBlock)
			{
				terminationReason = JitBlockInfo::TerminationReason::InstructionLimit;
				break;
			}
		}
	}

	bool JitBlock::emit(JitBlockRuntimeData& _rt, JitBlockChain* _chain, const TWord _pc, const std::vector<JitCacheEntry>& _cache, const std::set<TWord>& _volatileP, const std::map<TWord, TWord>& _loopStarts, const std::set<TWord>& _loopEnds, bool _profilingSupport)
	{
		JitBlockGenerating generating(_rt);
		m_currentJitBlockRuntimeData = &_rt;

		auto& dspAsm = _rt.m_dspAsm;
		auto& info = _rt.m_info;
		auto& profilingInfo = _rt.m_profilingInfo;
		auto& childAddr = _rt.m_child;

		m_chain = _chain;

		const bool isFastInterrupt = _pc < Vba_End;
		const auto fastInterruptMode = isFastInterrupt ? (m_config.dynamicFastInterrupts ? JitOps::FastInterruptMode::Dynamic : JitOps::FastInterruptMode::Static) : JitOps::FastInterruptMode::None;

		dspAsm.clear();

		// needed so that the dsp register is available
		m_asm.lea_(regDspPtr, g_funcArgGPs[0], Jitmem::pointerOffset(&m_dsp.regs(), &m_dsp.getJit()));
		dspRegPool().makeDspPtr(&m_dsp.getInstructionCounter(), sizeof(TWord));

#ifdef HAVE_X86_64
		if constexpr (false)
		{
			const auto skip = m_asm.newLabel();
			RegScratch s(*this);
			m_asm.mov(s, asmjit::Imm(&m_dsp.regs()));
			m_asm.cmp(regDspPtr, s);
			m_asm.jz(skip);
			m_asm.int3();
			m_asm.bind(skip);
		}
#endif

		PushAllUsed pm(*this);

		auto loopBegin = m_asm.newNamedLabel("loopBegin");
		m_asm.bind(loopBegin);

		asmjit::BaseNode* cursorInsertIncreaseInstructionCount = m_asm.cursor();	// inserted later

		uint32_t blockFlags = 0;

		getInfo(info, dsp(), _pc, m_config, _cache, _volatileP, _loopStarts, _loopEnds);

		const auto pcNext = _pc + info.memSize;

		if(fastInterruptMode != JitOps::FastInterruptMode::Static && info.terminationReason != JitBlockInfo::TerminationReason::PopPC)
		{
			if(info.branchTarget == g_invalidAddress || info.branchIsConditional)
			{
				DspValue pc(*this, pcNext, DspValue::Immediate24);
				m_dspRegPool.write(PoolReg::DspPC, pc);
			}
		}

		TWord opA = 0;
		TWord opB = 0;

#if defined(_DEBUG)
		std::string opDisasm;
#endif

		uint32_t opPC = 0;

		uint32_t& ccrRead = info.ccrRead;
		uint32_t& ccrWrite = info.ccrWrite;
		uint32_t& ccrOverwrite = info.ccrOverwrite;

		_rt.m_encodedCycles = info.cycleCount;

		TWord pMemSize = 0;

		while(pMemSize < info.memSize)
		{
			opPC = _pc + pMemSize;

			m_dsp.memory().getOpcode(opPC, opA, opB);

#if defined(_DEBUG)
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

			dspAsm += opDisasm + '\n';
			m_asm.comment(("DSPasm: " + opDisasm).c_str());
#endif

			if(_profilingSupport)
			{
				const auto labelBegin = m_asm.newLabel();
				const auto labelEnd = m_asm.newLabel();

				m_asm.bind(labelBegin);

				const JitBlockRuntimeData::InstructionProfilingInfo pi{ opPC, opA, opB, 0, 1, labelBegin, labelEnd, 0, 0, std::string()};
				profilingInfo.emplace_back(pi);
			}

			JitOps ops(*this, _rt, fastInterruptMode);

			if (m_config.splitOpsByNops)	m_asm.nop();
			ops.emit(opPC, opA, opB);
			if (m_config.splitOpsByNops)	m_asm.nop();

			if (_profilingSupport)
			{
				auto& pi = profilingInfo.back();

				pi.opLen = ops.getOpSize();

				const auto& oi = g_opcodes[ops.getInstruction()];

				if(oi.flag(OpFlagRepImmediate) || oi.flag(OpFlagRepDynamic))
					pi.lineCount++;

				m_asm.bind(pi.labelAfter);
			}

			blockFlags |= ops.getResultFlags();
			
			_rt.m_singleOpWordA = opA;
			_rt.m_singleOpWordB = opB;

			pMemSize += ops.getOpSize();
			++_rt.m_encodedInstructionCount;

			_rt.m_lastOpSize = ops.getOpSize();

			ccrRead |= ops.getCCRRead();
			const auto ccrW = ops.getCCRWritten();
			const auto ccrO = ccrW & ~ccrRead;	// overwrites are writes that have no reads beforehand, i.e. any previous state is discarded

			ccrWrite |= ccrW;
			ccrOverwrite |= ccrO;
		}

		assert(_rt.getEncodedCycleCount() >= _rt.getEncodedInstructionCount());

		if (info.terminationReason == JitBlockInfo::TerminationReason::PopPC)
			blockFlags |= JitOps::PopPC;

		const auto isLoopStart = info.hasFlag(JitBlockInfo::Flags::IsLoopBodyBegin);
		const auto isLoopEnd = info.terminationReason == JitBlockInfo::TerminationReason::LoopEnd;
		const auto isLoopBody = isLoopStart && isLoopEnd;

		bool childIsConditional = false;

		JitBlockRuntimeData* child = nullptr;
		JitBlockRuntimeData* nonBranchChild = nullptr;

		const auto pcFirst = _rt.getInfo().pc;

		if(_chain)
		{
			if(info.terminationReason == JitBlockInfo::TerminationReason::Branch && !info.hasFlag(JitBlockInfo::Flags::ModeChange))
			{
				// if the last instruction of a JIT block is a branch to an address known at compile time, and this branch is fixed, i.e. is not dependent
				// on a condition: Store that address to be able to call the next JIT block from the current block without having to have a transition to the C++ code

				const auto branchTarget = info.branchTarget;

				if (branchTarget != g_invalidAddress)
				{
					assert(branchTarget == g_dynamicAddress || branchTarget < m_dsp.memory().sizeP());

					const auto pcLast = pcFirst + pMemSize;

					childIsConditional = info.branchIsConditional;

					if (branchTarget == g_dynamicAddress)
					{
						childAddr = g_dynamicAddress;
					}
					// do not branch into ourselves
					else if (branchTarget < pcFirst || branchTarget >= pcLast)
					{
						child = _chain->getChildBlock(&_rt, branchTarget);

						if (child)
						{
							assert(child->getFunc());

							child->addParent(pcFirst);
							childAddr = branchTarget;

							if (childIsConditional)
							{
								nonBranchChild = _chain->getChildBlock(&_rt, pcLast);
								if (nonBranchChild)
								{
									nonBranchChild->addParent(pcFirst);
									_rt.m_nonBranchChild = pcLast;
								}
							}
						}
					}
				}
			}
			else if(!isLoopBody)
			{
				if ((info.terminationReason == JitBlockInfo::TerminationReason::ExistingCode || info.terminationReason == JitBlockInfo::TerminationReason::VolatileP))
				{
					if(!isFastInterrupt && !blockFlags && !isLoopEnd)
					{
						nonBranchChild = _chain->getChildBlock(&_rt, pcNext, false);
						if(nonBranchChild)
						{
							childAddr = pcNext;
							nonBranchChild->addParent(pcFirst);
						}
					}
				}
			}
		}

		m_asm.setCursor(cursorInsertIncreaseInstructionCount);
		increaseInstructionCount(asmjit::Imm(_rt.getEncodedInstructionCount()));
		increaseCycleCount(asmjit::Imm(_rt.getEncodedCycleCount()));
		m_asm.setCursor(m_asm.lastNode());

		auto jumpIfLoop = [&](const asmjit::Label& _ifTrue, const JitReg32& _regPC, const JitReg32& _regLC, const JitReg32& _temp)
		{
			if (!isLoopBody)
				return false;

			const SkipLabel skip(m_asm);

			if(m_config.maxDoIterations)
			{
				assert(asmjit::Support::isPowerOf2(m_config.maxDoIterations));
				m_asm.test_(_regLC, asmjit::Imm(m_config.maxDoIterations-1));
				m_asm.jz(skip);
			}

#ifdef HAVE_ARM64
			RegGP temp(*this, false);
			JitReg32 t;
			if(_temp.isValid())
			{
				t = _temp;
			}
			else
			{
				temp.acquire();
				t = r32(temp);
			}
			m_asm.mov(t, asmjit::Imm(pcFirst));
			m_asm.cmp(_regPC, t);
#else
			m_asm.cmp(_regPC, asmjit::Imm(pcFirst));
#endif
			m_asm.jz(_ifTrue);
			return true;
		};

		auto profileBegin = [&](const std::string& _name)
		{
			if(!_profilingSupport)
				return asmjit::Label();

			const auto labelBegin = m_asm.newLabel();
			const auto labelEnd = m_asm.newLabel();

			m_asm.bind(labelBegin);

			const JitBlockRuntimeData::InstructionProfilingInfo pi{ g_invalidAddress, 0, 0, 0, 1, labelBegin, labelEnd, 0, 0, _name};
			profilingInfo.emplace_back(pi);

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

			JitOps ops(*this, _rt, fastInterruptMode);

			// It is important that this code does not allocate any temp registers inside the branches. thefore, we prewarm everything
			RegGP temp(*this);

			const auto& sr = r32(m_dspRegPool.get(PoolReg::DspSR, true, true));
			                 r32(m_dspRegPool.get(PoolReg::DspLA, true, true));	// we don't use it here but do_end does
			const auto& lc = r32(m_dspRegPool.get(PoolReg::DspLC, true, true));

			m_dspRegPool.lock(PoolReg::DspSR);
			m_dspRegPool.lock(PoolReg::DspLA);
			m_dspRegPool.lock(PoolReg::DspLC);

			DSPReg pc(*this, PoolReg::DspPC, true, true);

			// check loop flag

			m_asm.bitTest(sr, SRB_LF);
			m_asm.jz(skip);

			m_asm.cmp(lc, asmjit::Imm(1));
			m_asm.jle(enddo);
			m_asm.dec(lc);

			if(isLoopBody)
			{
				m_asm.mov(r32(pc), asmjit::Imm(pcFirst));
			}
			else
			{
				const auto ss = r64(pc);
				m_dspRegs.getSS(ss);				// note: not calling getSSH as it will dec the SP
				m_asm.shr(ss, asmjit::Imm(24));
			}
			m_asm.jmp(skip);

			m_asm.bind(enddo);
			ops.do_end(temp);

			m_asm.bind(skip);

			m_dspRegPool.unlock(PoolReg::DspSR);
			m_dspRegPool.unlock(PoolReg::DspLA);
			m_dspRegPool.unlock(PoolReg::DspLC);

			profileEnd(pl);
		}

		auto ccrDirty = m_dspRegs.ccrDirtyFlags();

		if(ccrDirty)
		{
			// we can omit CCR updates for all CCR bits that are overwritten by child blocks
			uint32_t ccrOverwritten = 0;
			if(child)
			{
				ccrOverwritten = child->getInfo().ccrOverwrite;
				if(nonBranchChild)
					ccrOverwritten &= nonBranchChild->getInfo().ccrOverwrite;
			}
			else if(nonBranchChild)
			{
				ccrOverwritten = nonBranchChild->getInfo().ccrOverwrite;
			}

			ccrDirty = static_cast<CCRMask>(ccrDirty & ~ccrOverwritten);

			if(ccrDirty)
			{
				auto pl = profileBegin("ccrUpdate");

				asmjit::Label skipCCRupdate = m_asm.newLabel();

				// We skip to update dirty CCRs if we are running a loop and the loop has not yet ended.
				// Be sure to not skip CCR updates if the loop itself reads the SR before it has written to it
				const auto writesSRbeforeRead = info.hasFlag(JitBlockInfo::Flags::WritesSRbeforeRead);
				const auto readsSR = (info.readRegs & RegisterMask::SR) != RegisterMask::None;
				if(isLoopBody && (writesSRbeforeRead || !readsSR))
				{
					jumpIfLoop(skipCCRupdate, 
						r32(m_dspRegPool.get(PoolReg::DspPC, true, false)), 
						r32(m_dspRegPool.get(PoolReg::DspLC, true, false)), 
						JitReg32()
						);
				}

				JitOps op(*this, _rt, fastInterruptMode);

				op.updateDirtyCCR(ccrDirty);

				m_asm.bind(skipCCRupdate);

				profileEnd(pl);
			}
		}

		auto pl = profileBegin("release");

		RegScratch scratch(*this, false);

		JitReg32 regPC;

		if((child && childIsConditional) || isLoopBody)
		{
			regPC = r32(m_dspRegPool.get(PoolReg::DspPC, true, false));

			// we can keep our PC reg if its volatile. If It's not, it will be destroyed on stack.popAll() below => we need to copy it to a safe place
			// Also, we need to make sure that our PC reg is not the first function argument because we replace it with the Jit*
			if(JitStackHelper::isNonVolatile(regPC) || r64(regPC) == g_funcArgGPs[0])
			{
				scratch.acquire();
				asm_().mov(r32(scratch), regPC);
				regPC = r32(scratch);
			}
		}

		JitReg32 regLC;
		RegGP tempLC(*this, false);

		if(isLoopBody && m_config.maxDoIterations)
		{
			regLC = r32(m_dspRegPool.get(PoolReg::DspLC, true, false));

			// Basically the same rules that we use for the PC reg above apply to LC too:
			// we can keep our LC reg if its volatile. If It's not, it will be destroyed on stack.popAll() below => we need to copy it to a safe place
			// Also, we need to make sure that our LC reg is not the first function argument because we replace it with the Jit*
			if(JitStackHelper::isNonVolatile(regLC) || r64(regLC) == g_funcArgGPs[0])
			{
				std::vector<RegGP> temps;

				// acquire a temp that is volatile
				while(!m_gpPool.empty())
				{
					temps.emplace_back(*this);
					if(JitStackHelper::isNonVolatile(temps.back()))
						continue;

					tempLC = std::move(temps.back());
					temps.clear();
					break;
				}

				assert(tempLC.isValid());
				assert(!JitStackHelper::isNonVolatile(tempLC));

				asm_().mov(r32(tempLC), regLC);
				regLC = r32(tempLC);
			}
		}

		m_dspRegPool.releaseAll();

		pm.end();

		jumpIfLoop(loopBegin, regPC, regLC, regLC);

		m_stack.popAll();

		m_stack.reset();

		profileEnd(pl);

		asmjit::Label lj;

		if(child || nonBranchChild)
		{
			lj = profileBegin("jump");
			// first func arg needs to point to Jit*
			asm_().lea_(g_funcArgGPs[0], regDspPtr, Jitmem::pointerOffset(&dsp().getJit(), &dsp().regs()));
		}

		if (child)
		{
			if (childIsConditional)
			{
				// we need to check if the PC has been set to the target address
#ifdef HAVE_ARM64
				auto regTempImm = r32((regPC == r32(g_funcArgGPs[1])) ? g_funcArgGPs[2] : g_funcArgGPs[1]);
				m_asm.mov(regTempImm, asmjit::Imm(childAddr));
				m_asm.cmp(regPC, regTempImm);
#else
				m_asm.cmp(regPC, asmjit::Imm(childAddr));
#endif

				jumpToChild(child, JitCondCode::kZero);

				if(nonBranchChild)
					jumpToChild(nonBranchChild);
			}
			else
			{
				jumpToChild(child);
			}
		}
		else if(nonBranchChild)
		{
			jumpToChild(nonBranchChild);
		}

		profileEnd(lj);

		m_currentJitBlockRuntimeData = nullptr;
		return true;
	}

	void JitBlock::setNextPC(const DspValue& _pc)
	{
		m_dspRegPool.write(PoolReg::DspPC, _pc);
	}

	void JitBlock::increaseInstructionCount(const asmjit::Operand& _count)
	{
		increaseUint32(_count, m_dsp.getInstructionCounter());
	}

	void JitBlock::increaseCycleCount(const asmjit::Operand& _count)
	{
		increaseUint32(_count, m_dsp.getCycles());
	}

	void JitBlock::increaseUint32(const asmjit::Operand& _count, const uint32_t& _target)
	{
		const auto ptr = dspRegPool().makeDspPtr(&_target, sizeof(TWord));

#ifdef HAVE_ARM64
		const RegScratch scratch(*this);
		const auto r = r32(scratch);
		m_asm.ldr(r, ptr);
		if (_count.isImm())
		{
			// for rep loops, it is possible that the immediate exceeds 12 bits
			const auto& imm = _count.as<asmjit::Imm>();
			if(imm.value() > 0xfff)
			{
				assert(imm.value() <= 0xffffff);
				m_asm.add(r, r, asmjit::Imm(imm.value() & 0x000fff));
				m_asm.add(r, r, asmjit::Imm(imm.value() & 0xfff000));
			}
			else
			{
				m_asm.add(r, r, imm);
			}
		}
		else
		{
			m_asm.add(r, r, _count.as<JitRegGP>());
		}
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
		const auto* mode = getMode();
		return mode ? mode->getAddressingMode(_aguIndex) : AddressingMode::Unknown;
	}

	const JitDspMode* JitBlock::getMode() const
	{
		return m_chain ? &m_chain->getMode() : m_mode;
	}

	void JitBlock::setMode(JitDspMode* _mode)
	{
		m_mode = _mode;
	}

	void JitBlock::reset(JitConfig&& _config)
	{
		m_config = std::move(_config);
		m_stack.reset();
		m_xmmPool.reset({regXMMTempA});
		m_gpPool.reset(g_regGPTemps);
		m_dspRegs.reset();
		m_dspRegPool.reset();
		m_scratchLocked = false;
		m_shiftLocked = false;
	}

	JitBlock::JitBlockGenerating::JitBlockGenerating(JitBlockRuntimeData& _block): m_block(_block)
	{
		_block.setGenerating(true);
	}

	JitBlock::JitBlockGenerating::~JitBlockGenerating()
	{
		m_block.setGenerating(false);
	}

	void JitBlock::jumpToChild(const JitBlockRuntimeData* _child, const JitCondCode _cc/* = JitCondCode::kMaxValue*/) const
	{
		auto* p = asmjit::func_as_ptr(_child->getFunc());
		const auto addr = reinterpret_cast<uint64_t>(p);

		const auto tempReg = r64(g_funcArgGPs[1]);

		auto initTemp = [&]()
		{
			if(const auto offset = Jitmem::pointerOffset(p, &m_dsp.regs()))
				m_asm.lea_(tempReg, regDspPtr, offset);
			else
				m_asm.mov(tempReg, asmjit::Imm(addr));
			return tempReg;
		};

		if(_cc == JitCondCode::kMaxValue)
		{
#ifdef HAVE_ARM64
			m_asm.br(initTemp());
#else
			m_asm.jmp(initTemp());
#endif
		}
		else
		{
			// neither x86 nor ARM have a conditional jump to register
			const auto l = m_asm.newLabel();
			const auto cc = JitOps::reverseCC(_cc);

#ifdef HAVE_ARM64
			m_asm.b(cc, l);
			m_asm.br(initTemp());
#else
			m_asm.j(cc, l);
			m_asm.jmp(initTemp());
#endif
			m_asm.bind(l);
		}
	}
}
