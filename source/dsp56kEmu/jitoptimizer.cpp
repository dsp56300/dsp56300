#include "jitoptimizer.h"

#if defined(_WIN32) && defined(_DEBUG)
#include "dsp56kBase/logging.h"
#endif

#include <asmjit/asmjit.h>

#include "jitemitter.h"

namespace dsp56k
{
	using InstId = asmjit::InstId;

#ifdef HAVE_ARM64
	using Inst = asmjit::a64::Inst;
#else
	namespace Inst = asmjit::x86::Inst;
#endif

	JitOptimizer::JitOptimizer(JitEmitter& _emitter) : m_asm(_emitter)
	{
	}

	uint32_t JitOptimizer::regIndex(uint32_t _group, uint32_t _id)
	{
		if(_group == static_cast<uint32_t>(asmjit::RegGroup::kGp))
			return kGpBase + (_id < 32 ? _id : 31);
		if(_group == static_cast<uint32_t>(asmjit::RegGroup::kVec))
			return kVecBase + (_id < 32 ? _id : 31);
		return kFlagsIdx;
	}

	uint32_t JitOptimizer::regIndex(const asmjit::Operand& _op, bool& _valid)
	{
		if(!_op.isReg())
		{
			_valid = false;
			return 0;
		}
		const auto& reg = _op.as<asmjit::BaseReg>();
		if(!reg.isPhysReg() || (!reg.isGp() && !reg.isVec()))
		{
			_valid = false;
			return 0;
		}
		_valid = true;
		return regIndex(static_cast<uint32_t>(reg.group()), reg.id());
	}

	size_t JitOptimizer::optimize()
	{
		size_t total = 0;

		// Run constant folding then dead code elimination in a loop.
		// Constant folding creates dead code (e.g. mov reg,reg replaced with
		// mov reg,imm makes the source's mov dead).  DCE may expose further
		// folding opportunities.  Iterate until stable.
		for(;;)
		{
			const size_t changed = constantFolding() + deadCodeElimination();
			total += changed;
			if(changed == 0)
				break;
		}

#if defined(_WIN32) && defined(_DEBUG)
		if(total > 0)
			LOG("JitOptimizer: " << total << " optimizations applied");
#endif

		return total;
	}

	int64_t JitOptimizer::maskToRegSize(const asmjit::Operand& _reg, int64_t _value)
	{
		if(!_reg.isReg())
			return _value;
		const auto size = _reg.as<asmjit::BaseReg>().size();
		if(size <= 4)
			return static_cast<int64_t>(static_cast<uint32_t>(_value));
		return _value;
	}

	bool JitOptimizer::areFlagsLive(const asmjit::BaseNode* _node) const
	{
		const auto arch = asmjit::Environment::host().arch();

		for(auto* n = _node->next(); n; n = n->next())
		{
			// Labels don't modify flags — skip them.
			// Only actual control flow (branches, calls) is a boundary.
			if(n->isLabel())
				continue;

			if(isControlFlow(n))
				return true;

			if(!n->isInst())
				continue;

			const auto* inst = n->as<asmjit::InstNode>();
			asmjit::InstRWInfo rw{};
			if(asmjit::InstAPI::queryRWInfo(arch, inst->baseInst(), inst->operands(), inst->opCount(), &rw) != asmjit::kErrorOk)
				return true;

			if(readsFlags(inst, rw))
				return true;

			if(writesFlags(inst, rw))
				return false;
		}

		return true;
	}

	// Dead code elimination using backward liveness analysis within basic blocks.
	// An instruction is dead if it only writes to registers (and/or flags) that are
	// not in the live set, and has no other side effects.
	size_t JitOptimizer::deadCodeElimination() const
	{
		const auto arch = asmjit::Environment::host().arch();
		size_t removed = 0;

		// Track live registers as a bitset — zero heap allocations.
		RegBits liveRegs;

		auto setAllLive = [&]()
		{
			liveRegs.clear();
			// Mark all GP and vector registers as live (conservative).
#ifdef HAVE_ARM64
			constexpr uint32_t gpCount = 32;  // x0-x30 + sp/xzr
#else
			constexpr uint32_t gpCount = 16;
#endif
			for(uint32_t id = 0; id < gpCount; ++id)
				liveRegs.set(kGpBase + id);
			for(uint32_t id = 0; id < 32; ++id)
				liveRegs.set(kVecBase + id);
			liveRegs.set(kFlagsIdx);
		};

		setAllLive();  // conservative at end of function

		// Walk the node list backward directly — no vector copy needed
		for(auto* node = m_asm.lastNode(); node; node = node->prev())
		{
			// Labels are just markers — skip them.  Only actual control flow
			// (branches, calls) requires conservative liveness.
			if(node->isLabel())
				continue;

			if(isControlFlow(node))
			{
				setAllLive();
				continue;
			}

			if(!node->isInst())
				continue;

			auto* inst = node->as<asmjit::InstNode>();

			asmjit::InstRWInfo rwInfo{};
			const auto err = asmjit::InstAPI::queryRWInfo(arch, inst->baseInst(), inst->operands(), inst->opCount(), &rwInfo);
			if(err != asmjit::kErrorOk)
				continue;

			if(!isSideEffectFree(inst, rwInfo))
			{
				// This instruction has side effects; conservatively mark all
				// register operands as live.
				for(uint32_t i = 0; i < inst->opCount(); ++i)
				{
					bool valid;
					const auto idx = regIndex(inst->op(i), valid);
					if(valid)
						liveRegs.set(idx);

					if(inst->op(i).isMem())
					{
						const auto& mem = inst->op(i).as<asmjit::BaseMem>();
						if(mem.hasBaseReg())
							liveRegs.set(regIndex(static_cast<uint32_t>(asmjit::RegGroup::kGp), mem.baseId()));
						if(mem.hasIndexReg())
							liveRegs.set(regIndex(static_cast<uint32_t>(asmjit::RegGroup::kGp), mem.indexId()));
					}
				}

				if(readsFlags(inst, rwInfo))
					liveRegs.set(kFlagsIdx);

				continue;
			}

			// Side-effect-free instruction: check if all written outputs are dead.
			// Use a fixed stack array instead of std::vector (max 6 operands).
			bool anyWrittenLive = false;
			uint32_t writtenIndices[6];
			uint32_t writtenCount = 0;

			for(uint32_t i = 0; i < inst->opCount(); ++i)
			{
				bool valid;
				const auto idx = regIndex(inst->op(i), valid);
				if(!valid)
					continue;

				bool isWrite = false;
				if(i < rwInfo.opCount())
				{
					const auto& opInfo = rwInfo.operand(i);
					isWrite = opInfo.isWrite() || opInfo.isReadWrite();
				}

				if(isWrite)
				{
					writtenIndices[writtenCount++] = idx;
					if(liveRegs.test(idx))
						anyWrittenLive = true;
				}
			}

			if(writesFlags(inst, rwInfo))
			{
				if(liveRegs.test(kFlagsIdx))
					anyWrittenLive = true;
			}

			if(!anyWrittenLive && writtenCount > 0)
			{
				m_asm.removeNode(node);
				++removed;
				continue;
			}

			// Instruction is not dead. Update liveness.
			for(uint32_t i = 0; i < inst->opCount(); ++i)
			{
				bool valid;
				const auto idx = regIndex(inst->op(i), valid);
				if(valid)
				{
					if(i < rwInfo.opCount())
					{
						const auto& opInfo = rwInfo.operand(i);
						if(opInfo.isWriteOnly())
							liveRegs.clr(idx);
						if(opInfo.isRead() || opInfo.isReadWrite())
							liveRegs.set(idx);
					}
					else
					{
						liveRegs.set(idx);
					}
				}

				if(inst->op(i).isMem())
				{
					const auto& mem = inst->op(i).as<asmjit::BaseMem>();
					if(mem.hasBaseReg())
						liveRegs.set(regIndex(static_cast<uint32_t>(asmjit::RegGroup::kGp), mem.baseId()));
					if(mem.hasIndexReg())
						liveRegs.set(regIndex(static_cast<uint32_t>(asmjit::RegGroup::kGp), mem.indexId()));
				}
			}

			if(writesFlags(inst, rwInfo))
				liveRegs.clr(kFlagsIdx);
			if(readsFlags(inst, rwInfo))
				liveRegs.set(kFlagsIdx);

		}

		return removed;
	}

	// Constant folding: track known constant values in registers and evaluate
	// arithmetic at compile time when all inputs are constants.
	size_t JitOptimizer::constantFolding()
	{
		const auto arch = asmjit::Environment::host().arch();
		size_t folded = 0;

		ConstMap constants;
		LabelConstMap labelConstants;

		for(auto* node = m_asm.firstNode(); node;)
		{
			auto* next = node->next();

			// At control flow, save the pre-branch constants so they can be
			// intersected at the target label.  This allows folding through
			// if/else constructs where the constant is set before the branch.
			if(isControlFlow(node))
			{
				bool isConditional = false;

				if(node->isInst())
				{
					const auto* cfInst = node->as<asmjit::InstNode>();
					for(uint32_t i = 0; i < cfInst->opCount(); ++i)
					{
						if(cfInst->op(i).isLabel())
						{
							isConditional = true;
							const auto labelId = cfInst->op(i).as<asmjit::Label>().id();
							auto* existing = labelConstants.find(labelId);
							if(!existing)
							{
								auto& m = labelConstants.insert(labelId);
								m = constants;
							}
							else
							{
								existing->intersect(constants);
							}
						}
					}
				}

				// Conditional branches: fall-through keeps all constants.
				// Unconditional jumps/calls/ret: clear all (code after is dead or unknown).
				if(!isConditional)
					constants.clear();

				node = next;
				continue;
			}

			if(node->isLabel())
			{
				// At labels: intersect current (fall-through) constants with
				// the saved branch-path constants.
				const auto labelId = node->as<asmjit::LabelNode>()->labelId();
				auto* saved = labelConstants.find(labelId);
				if(saved)
				{
					constants.intersect(*saved);
					labelConstants.erase(labelId);
				}
				else
				{
					// No branch targets this label — it's only reached via
					// fall-through.  Preserve current constants.
				}
				node = next;
				continue;
			}

			if(!node->isInst())
			{
				node = next;
				continue;
			}

			auto* inst = node->as<asmjit::InstNode>();
			const auto instId = inst->id();
			const auto opCount = inst->opCount();

			asmjit::InstRWInfo rwInfo{};
			const auto err = asmjit::InstAPI::queryRWInfo(arch, inst->baseInst(), inst->operands(), opCount, &rwInfo);
			if(err != asmjit::kErrorOk)
			{
				node = next;
				continue;
			}

			const bool flagsAreLive = writesFlags(inst, rwInfo) && areFlagsLive(node);

			bool didFold = false;

			// Pattern: mov reg, imm -> track constant
#ifdef HAVE_ARM64
			if(instId == Inst::kIdMov || instId == Inst::kIdMovz || instId == Inst::kIdMovn)
#else
			if(instId == Inst::kIdMov)
#endif
			{
				if(opCount == 2 && inst->op(0).isReg() && inst->op(1).isImm())
				{
					bool valid;
					const auto idx = regIndex(inst->op(0), valid);
					if(valid)
						constants.set(idx, maskToRegSize(inst->op(0), inst->op(1).as<asmjit::Imm>().value()));
					node = next;
					continue;
				}

				// mov reg, reg -> propagate constant, replace with mov reg, imm if known
				if(opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
				{
					bool dstValid, srcValid;
					const auto dstIdx = regIndex(inst->op(0), dstValid);
					const auto srcIdx = regIndex(inst->op(1), srcValid);
					if(dstValid && srcValid)
					{
						if(constants.has(srcIdx))
						{
							const auto value = maskToRegSize(inst->op(0), constants.get(srcIdx));
							constants.set(dstIdx, value);
							inst->setOp(1, asmjit::Imm(value));
							++folded;
						}
						else
							constants.erase(dstIdx);
					}
					node = next;
					continue;
				}
			}

			// Binary operations: op dst, imm  (where dst is also a source, i.e. read-write)
			// If dst holds a known constant, fold the operation
#ifdef HAVE_ARM64
			if(instId == Inst::kIdAdd || instId == Inst::kIdSub ||
			   instId == Inst::kIdAnd || instId == Inst::kIdOrr || instId == Inst::kIdEor ||
			   instId == Inst::kIdLsl || instId == Inst::kIdLsr || instId == Inst::kIdAsr)
#else
			if(instId == Inst::kIdAdd || instId == Inst::kIdSub ||
			   instId == Inst::kIdAnd || instId == Inst::kIdOr || instId == Inst::kIdXor ||
			   instId == Inst::kIdShl || instId == Inst::kIdShr || instId == Inst::kIdSar ||
			   instId == Inst::kIdRol || instId == Inst::kIdRor || instId == Inst::kIdRorx)
#endif
			{
				if(opCount == 2 && inst->op(0).isReg() && inst->op(1).isImm())
				{
					bool valid;
					const auto dstIdx = regIndex(inst->op(0), valid);
					if(valid && constants.has(dstIdx))
					{
						{
							const auto regVal = constants.get(dstIdx);
							const auto immVal = inst->op(1).as<asmjit::Imm>().value();
							int64_t result = 0;
							bool canFold = true;

#ifdef HAVE_ARM64
							if     (instId == Inst::kIdAdd) result = regVal + immVal;
							else if(instId == Inst::kIdSub) result = regVal - immVal;
							else if(instId == Inst::kIdAnd) result = regVal & immVal;
							else if(instId == Inst::kIdOrr) result = regVal | immVal;
							else if(instId == Inst::kIdEor) result = regVal ^ immVal;
							else if(instId == Inst::kIdLsl) result = regVal << (immVal & 63);
							else if(instId == Inst::kIdLsr) result = static_cast<int64_t>(static_cast<uint64_t>(regVal) >> (immVal & 63));
							else if(instId == Inst::kIdAsr) result = regVal >> (immVal & 63);
							else canFold = false;
#else
							if     (instId == Inst::kIdAdd) result = regVal + immVal;
							else if(instId == Inst::kIdSub) result = regVal - immVal;
							else if(instId == Inst::kIdAnd) result = regVal & immVal;
							else if(instId == Inst::kIdOr)  result = regVal | immVal;
							else if(instId == Inst::kIdXor) result = regVal ^ immVal;
							else if(instId == Inst::kIdShl) result = regVal << (immVal & 63);
							else if(instId == Inst::kIdShr) result = static_cast<int64_t>(static_cast<uint64_t>(regVal) >> (immVal & 63));
							else if(instId == Inst::kIdSar) result = regVal >> (immVal & 63);
							else if(instId == Inst::kIdRol) result = static_cast<int64_t>((static_cast<uint64_t>(regVal) << (immVal & 63)) | (static_cast<uint64_t>(regVal) >> (64 - (immVal & 63))));
							else if(instId == Inst::kIdRor) result = static_cast<int64_t>((static_cast<uint64_t>(regVal) >> (immVal & 63)) | (static_cast<uint64_t>(regVal) << (64 - (immVal & 63))));
							else canFold = false;
#endif
							if(canFold && !flagsAreLive)
							{
								// Replace with: mov dst, result
								inst->setId(Inst::kIdMov);
								inst->setOpCount(2);
								inst->setOp(1, asmjit::Imm(result));
								constants.set(dstIdx, maskToRegSize(inst->op(0), result));
								didFold = true;
								++folded;
							}
						}
					}
				}

				// Binary operation: op dst, reg where both dst and reg are known constants
				// This handles patterns like:  add dst, src  where both hold known values
				if(!didFold && opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
				{
					bool dstValid, srcValid;
					const auto dstIdx = regIndex(inst->op(0), dstValid);
					const auto srcIdx = regIndex(inst->op(1), srcValid);
					if(dstValid && srcValid && constants.has(dstIdx) && constants.has(srcIdx))
					{
						{
							const auto dstVal = constants.get(dstIdx);
							const auto srcVal = constants.get(srcIdx);
							int64_t result = 0;
							bool canFold = true;

#ifdef HAVE_ARM64
							if     (instId == Inst::kIdAdd) result = dstVal + srcVal;
							else if(instId == Inst::kIdSub) result = dstVal - srcVal;
							else if(instId == Inst::kIdAnd) result = dstVal & srcVal;
							else if(instId == Inst::kIdOrr) result = dstVal | srcVal;
							else if(instId == Inst::kIdEor) result = dstVal ^ srcVal;
							else canFold = false;
#else
							if     (instId == Inst::kIdAdd) result = dstVal + srcVal;
							else if(instId == Inst::kIdSub) result = dstVal - srcVal;
							else if(instId == Inst::kIdAnd) result = dstVal & srcVal;
							else if(instId == Inst::kIdOr)  result = dstVal | srcVal;
							else if(instId == Inst::kIdXor) result = dstVal ^ srcVal;
							else canFold = false;
#endif
							if(canFold && !flagsAreLive)
							{
								inst->setId(Inst::kIdMov);
								inst->setOpCount(2);
								inst->setOp(1, asmjit::Imm(result));
								constants.set(dstIdx, maskToRegSize(inst->op(0), result));
								didFold = true;
								++folded;
							}
						}
					}
				}

#ifndef HAVE_ARM64
				// x86 three-operand form: op dst, src, imm (e.g. imul)
				if(!didFold && opCount == 3 && inst->op(0).isReg() && inst->op(1).isReg() && inst->op(2).isImm())
				{
					bool srcValid;
					const auto srcIdx = regIndex(inst->op(1), srcValid);
					if(srcValid && constants.has(srcIdx))
					{
						{
							const auto srcVal = constants.get(srcIdx);
							const auto immVal = inst->op(2).as<asmjit::Imm>().value();
							int64_t result = 0;
							bool canFold = true;

							// Three-operand forms on x86 are less common for basic arithmetic
							// but handle the pattern if encountered
							if     (instId == Inst::kIdAdd) result = srcVal + immVal;
							else if(instId == Inst::kIdSub) result = srcVal - immVal;
							else if(instId == Inst::kIdAnd) result = srcVal & immVal;
							else if(instId == Inst::kIdOr)  result = srcVal | immVal;
							else if(instId == Inst::kIdXor) result = srcVal ^ immVal;
							else if(instId == Inst::kIdShl) result = srcVal << (immVal & 63);
							else if(instId == Inst::kIdShr) result = static_cast<int64_t>(static_cast<uint64_t>(srcVal) >> (immVal & 63));
							else if(instId == Inst::kIdSar) result = srcVal >> (immVal & 63);
							else if(instId == Inst::kIdRorx) result = static_cast<int64_t>((static_cast<uint64_t>(srcVal) >> (immVal & 63)) | (static_cast<uint64_t>(srcVal) << (64 - (immVal & 63))));
							else canFold = false;

							if(canFold && !flagsAreLive)
							{
								bool dstValid;
								const auto dstIdx2 = regIndex(inst->op(0), dstValid);
								if(dstValid)
								{
									inst->setId(Inst::kIdMov);
									inst->setOpCount(2);
									inst->setOp(0, inst->op(0));
									inst->setOp(1, asmjit::Imm(result));
									inst->resetOp(2);
									constants.set(dstIdx2, maskToRegSize(inst->op(0), result));
									didFold = true;
									++folded;
								}
							}
						}
					}
				}
#endif
			}

#ifndef HAVE_ARM64
			// x86: xor reg, reg -> constant 0
			if(instId == Inst::kIdXor && opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
			{
				bool v0, v1;
				const auto i0 = regIndex(inst->op(0), v0);
				const auto i1 = regIndex(inst->op(1), v1);
				if(v0 && v1 && i0 == i1)
				{
					constants.set(i0, 0);
					node = next;
					continue;
				}
			}

			// x86: neg reg -> negate constant
			if(!flagsAreLive && instId == Inst::kIdNeg && opCount == 1 && inst->op(0).isReg())
			{
				bool valid;
				const auto idx = regIndex(inst->op(0), valid);
				if(valid && constants.has(idx))
				{
					const auto result = -constants.get(idx);
					inst->setId(Inst::kIdMov);
					inst->setOpCount(2);
					inst->setOp(1, asmjit::Imm(result));
					constants.set(idx, maskToRegSize(inst->op(0), result));
					++folded;
					didFold = true;
				}
			}

			// x86: not reg -> bitwise not constant
			if(!flagsAreLive && instId == Inst::kIdNot && opCount == 1 && inst->op(0).isReg())
			{
				bool valid;
				const auto idx = regIndex(inst->op(0), valid);
				if(valid && constants.has(idx))
				{
					const auto result = ~constants.get(idx);
					inst->setId(Inst::kIdMov);
					inst->setOpCount(2);
					inst->setOp(1, asmjit::Imm(result));
					constants.set(idx, maskToRegSize(inst->op(0), result));
					++folded;
					didFold = true;
				}
			}
#else
			// ARM64: neg reg, reg -> negate constant
			if(instId == Inst::kIdNeg && opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
			{
				bool srcValid, dstValid;
				const auto srcIdx = regIndex(inst->op(1), srcValid);
				const auto dstIdx = regIndex(inst->op(0), dstValid);
				if(srcValid && constants.has(srcIdx) && dstValid)
				{
					const auto result = -constants.get(srcIdx);
					inst->setId(Inst::kIdMov);
					inst->setOp(1, asmjit::Imm(result));
					constants.set(dstIdx, maskToRegSize(inst->op(0), result));
					++folded;
					didFold = true;
				}
			}

			// ARM64: mvn reg, reg -> bitwise not constant
			if(instId == Inst::kIdMvn && opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
			{
				bool srcValid, dstValid;
				const auto srcIdx = regIndex(inst->op(1), srcValid);
				const auto dstIdx = regIndex(inst->op(0), dstValid);
				if(srcValid && constants.has(srcIdx) && dstValid)
				{
					const auto result = ~constants.get(srcIdx);
					inst->setId(Inst::kIdMov);
					inst->setOp(1, asmjit::Imm(result));
					constants.set(dstIdx, maskToRegSize(inst->op(0), result));
					++folded;
					didFold = true;
				}
			}

			// ARM64 three-operand forms: op dst, src1, src2/imm
			if(!didFold &&
			   (instId == Inst::kIdAdd || instId == Inst::kIdSub ||
			    instId == Inst::kIdAnd || instId == Inst::kIdOrr || instId == Inst::kIdEor ||
			    instId == Inst::kIdLsl || instId == Inst::kIdLsr || instId == Inst::kIdAsr))
			{
				if(opCount == 3 && inst->op(0).isReg() && inst->op(1).isReg())
				{
					bool src1Valid;
					const auto src1Idx = regIndex(inst->op(1), src1Valid);
					if(src1Valid && constants.has(src1Idx))
					{
						const auto src1Val = constants.get(src1Idx);
						int64_t src2Val = 0;
						bool src2Known = false;

						if(inst->op(2).isImm())
						{
							src2Val = inst->op(2).as<asmjit::Imm>().value();
							src2Known = true;
						}
						else if(inst->op(2).isReg())
						{
							bool src2Valid;
							const auto src2Idx = regIndex(inst->op(2), src2Valid);
							if(src2Valid && constants.has(src2Idx))
							{
								src2Val = constants.get(src2Idx);
								src2Known = true;
							}
						}

						if(src2Known)
						{
							int64_t result = 0;
							bool canFold = true;

							if     (instId == Inst::kIdAdd) result = src1Val + src2Val;
							else if(instId == Inst::kIdSub) result = src1Val - src2Val;
							else if(instId == Inst::kIdAnd) result = src1Val & src2Val;
							else if(instId == Inst::kIdOrr) result = src1Val | src2Val;
							else if(instId == Inst::kIdEor) result = src1Val ^ src2Val;
							else if(instId == Inst::kIdLsl) result = src1Val << (src2Val & 63);
							else if(instId == Inst::kIdLsr) result = static_cast<int64_t>(static_cast<uint64_t>(src1Val) >> (src2Val & 63));
							else if(instId == Inst::kIdAsr) result = src1Val >> (src2Val & 63);
							else canFold = false;

							if(canFold && !flagsAreLive)
							{
								bool dstValid;
								const auto dstIdx = regIndex(inst->op(0), dstValid);
								if(dstValid)
								{
									inst->setId(Inst::kIdMov);
									inst->setOpCount(2);
									inst->setOp(1, asmjit::Imm(result));
									inst->resetOp(2);
									constants.set(dstIdx, maskToRegSize(inst->op(0), result));
									++folded;
									didFold = true;
								}
							}
						}
					}
				}
			}
#endif

			// If we didn't fold, update constant tracking based on what the instruction writes
			if(!didFold)
			{
				for(uint32_t i = 0; i < rwInfo.opCount() && i < opCount; ++i)
				{
					const auto& opInfo = rwInfo.operand(i);
					if(opInfo.isWrite() || opInfo.isReadWrite())
					{
						bool valid;
						const auto idx = regIndex(inst->op(i), valid);
						if(valid)
							constants.erase(idx);
					}
				}

				// Memory operations and calls may alias anything; be conservative
				// by invalidating constants stored in registers that may have been loaded from memory
				// Actually, only loads into registers need to invalidate the destination.
				// The above loop already handles that since loads write to a register.
				// But calls invalidate everything.
#ifdef HAVE_ARM64
				if(instId == Inst::kIdBlr || instId == Inst::kIdBl)
					constants.clear();
#else
				if(instId == Inst::kIdCall)
					constants.clear();
#endif
			}

			node = next;
		}

		return folded;
	}

	bool JitOptimizer::isSideEffectFree(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo) const
	{
		const auto instId = _inst->id();

		// Instructions that access memory have side effects (stores are observable, loads may have side effects)
		for(uint32_t i = 0; i < _inst->opCount(); ++i)
		{
			if(_inst->op(i).isMem())
				return false;
		}

		// Call instructions have side effects
#ifdef HAVE_ARM64
		if(instId == Inst::kIdBlr || instId == Inst::kIdBl)
			return false;
#else
		if(instId == Inst::kIdCall)
			return false;
#endif

		// Branch/jump instructions have side effects (control flow)
		if(isControlFlow(_inst))
			return false;

		// Stack manipulation has side effects
#ifdef HAVE_ARM64
		if(instId == Inst::kIdLdp || instId == Inst::kIdStp)
			return false;
#else
		if(instId == Inst::kIdPush || instId == Inst::kIdPop)
			return false;
#endif

		// System instructions
#ifdef HAVE_ARM64
		if(instId == Inst::kIdBrk || instId == Inst::kIdSvc || instId == Inst::kIdRet)
			return false;
#else
		if(instId == Inst::kIdInt || instId == Inst::kIdRet)
			return false;
#endif

		return true;
	}

	bool JitOptimizer::isControlFlow(const asmjit::BaseNode* _node) const
	{
		if(!_node->isInst())
			return false;

		const auto* inst = _node->as<asmjit::InstNode>();
		const auto instId = inst->id();

		// Check if any operand is a label (branch target)
		for(uint32_t i = 0; i < inst->opCount(); ++i)
		{
			if(inst->op(i).isLabel())
				return true;
		}

#ifdef HAVE_ARM64
		if(instId == Inst::kIdB || instId == Inst::kIdBl || instId == Inst::kIdBlr ||
		   instId == Inst::kIdBr || instId == Inst::kIdRet ||
		   instId == Inst::kIdCbz || instId == Inst::kIdCbnz ||
		   instId == Inst::kIdTbz || instId == Inst::kIdTbnz)
			return true;
#else
		if(instId == Inst::kIdJmp || instId == Inst::kIdCall || instId == Inst::kIdRet)
			return true;
		// x86 conditional jumps are in a range
		if(instId >= Inst::kIdJa && instId <= Inst::kIdJz)
			return true;
#endif

		return false;
	}

	bool JitOptimizer::writesFlags(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo)
	{
		if(_rwInfo.writeFlags() != asmjit::CpuRWFlags::kNone)
			return true;

#ifdef HAVE_ARM64
		// asmjit's queryRWInfo does not report writeFlags for ARM64 flag-setting
		// instructions (cmp, tst, adds, subs, ands, etc.).  Detect them manually.
		const auto instId = _inst->id();
		if(instId == Inst::kIdAdds || instId == Inst::kIdSubs ||
		   instId == Inst::kIdAnds || instId == Inst::kIdBics ||
		   instId == Inst::kIdAdcs || instId == Inst::kIdSbcs ||
		   instId == Inst::kIdCmp  || instId == Inst::kIdCmn  ||
		   instId == Inst::kIdTst  ||
		   instId == Inst::kIdCcmp || instId == Inst::kIdCcmn)
			return true;
#endif

		return false;
	}

	bool JitOptimizer::readsFlags(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo)
	{
		if(_rwInfo.readFlags() != asmjit::CpuRWFlags::kNone)
			return true;

#ifdef HAVE_ARM64
		// asmjit may not report readFlags for ARM64 conditional instructions.
		const auto instId = _inst->id();
		if(instId == Inst::kIdCsel  || instId == Inst::kIdCsinc ||
		   instId == Inst::kIdCsinv || instId == Inst::kIdCsneg ||
		   instId == Inst::kIdCset  || instId == Inst::kIdCsetm ||
		   instId == Inst::kIdCinc  || instId == Inst::kIdCinv  ||
		   instId == Inst::kIdCneg  ||
		   instId == Inst::kIdCcmp  || instId == Inst::kIdCcmn)
			return true;
#endif

		return false;
	}
}
