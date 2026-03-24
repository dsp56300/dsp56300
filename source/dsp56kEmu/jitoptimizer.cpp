#include "jitoptimizer.h"

#include "dsp56kBase/logging.h"

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

	size_t JitOptimizer::optimize()
	{
		size_t total = 0;

		// Run constant folding first, then peephole dead-mov elimination.
		// Constant folding may create consecutive mov reg,imm to the same register
		// (e.g. rorx+sar+shr chain folded to three movs — only the last matters).
		// Iterate until no more changes are made.
		for(;;)
		{
			const size_t changed = constantFolding() + removeDeadMovs();
			total += changed;
			if(changed == 0)
				break;
		}

		if(total > 0)
			LOG("JitOptimizer: " << total << " optimizations applied");

		return total;
	}

	bool JitOptimizer::getRegKey(RegKey& _result, const asmjit::Operand& _op)
	{
		if(!_op.isReg())
			return false;

		const auto& reg = _op.as<asmjit::BaseReg>();
		if(!reg.isPhysReg())
			return false;
		if(!reg.isGp() && !reg.isVec())
			return false;

		_result.group = static_cast<uint32_t>(reg.group());
		_result.id = reg.id();
		return true;
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

			if(rw.readFlags() != asmjit::CpuRWFlags::kNone)
				return true;

			if(rw.writeFlags() != asmjit::CpuRWFlags::kNone)
				return false;
		}

		return true;
	}

	// Peephole pass: remove "mov reg, imm" when the same register is immediately
	// overwritten by the next non-nop instruction (which also writes to reg
	// without reading it first).  This handles the pattern created by constant
	// folding where a chain like "rorx r10,r12,24; sar r10,8; shr r10,8" becomes
	// three consecutive "mov r10, ..." — only the last one matters.
	size_t JitOptimizer::removeDeadMovs()
	{
		const auto arch = asmjit::Environment::host().arch();
		size_t removed = 0;

		for(auto* node = m_asm.firstNode(); node;)
		{
			auto* next = node->next();

			if(!node->isInst() || node->isLabel())
			{
				node = next;
				continue;
			}

			auto* inst = node->as<asmjit::InstNode>();

#ifdef HAVE_ARM64
			if(inst->id() != asmjit::a64::Inst::kIdMov && inst->id() != asmjit::a64::Inst::kIdMovz)
#else
			if(inst->id() != asmjit::x86::Inst::kIdMov)
#endif
			{
				node = next;
				continue;
			}

			// Must be "mov reg, imm" (write-only to reg)
			if(inst->opCount() != 2 || !inst->op(0).isReg() || !inst->op(1).isImm())
			{
				node = next;
				continue;
			}

			RegKey key;
			if(!getRegKey(key, inst->op(0)))
			{
				node = next;
				continue;
			}

			// Look at the next non-nop instruction
			auto* nextInst = next;
			while(nextInst && nextInst->isInst() && nextInst->as<asmjit::InstNode>()->id() == asmjit::x86::Inst::kIdNop)
				nextInst = nextInst->next();

			if(!nextInst || !nextInst->isInst() || nextInst->isLabel())
			{
				node = next;
				continue;
			}

			// Check if the next instruction writes to the same register without reading it
			auto* nextI = nextInst->as<asmjit::InstNode>();
			asmjit::InstRWInfo rwInfo{};
			if(asmjit::InstAPI::queryRWInfo(arch, nextI->baseInst(), nextI->operands(), nextI->opCount(), &rwInfo) != asmjit::kErrorOk)
			{
				node = next;
				continue;
			}

			bool nextWritesSameReg = false;
			bool nextReadsSameReg = false;

			for(uint32_t i = 0; i < rwInfo.opCount() && i < nextI->opCount(); ++i)
			{
				RegKey nk;
				if(getRegKey(nk, nextI->op(i)) && nk == key)
				{
					const auto& opInfo = rwInfo.operand(i);
					if(opInfo.isWrite() || opInfo.isReadWrite())
						nextWritesSameReg = true;
					if(opInfo.isRead() || opInfo.isReadWrite())
						nextReadsSameReg = true;
				}

				// Also check memory operands for reads of the register
				if(nextI->op(i).isMem())
				{
					const auto& mem = nextI->op(i).as<asmjit::BaseMem>();
					if(mem.hasBaseReg() && mem.baseId() == key.id)
						nextReadsSameReg = true;
					if(mem.hasIndexReg() && mem.indexId() == key.id)
						nextReadsSameReg = true;
				}
			}

			if(nextWritesSameReg && !nextReadsSameReg)
			{
				// The next instruction overwrites this register without reading it.
				// This mov is dead.
				m_asm.removeNode(node);
				++removed;
			}

			node = next;
		}

		return removed;
	}

	// Dead code elimination using backward liveness analysis within basic blocks.
	// An instruction is dead if it only writes to registers (and/or flags) that are
	// not in the live set, and has no other side effects.
	size_t JitOptimizer::deadCodeElimination()
	{
		const auto arch = asmjit::Environment::host().arch();
		size_t removed = 0;

		// Collect nodes into basic blocks separated by labels and control flow.
		// Process each basic block independently in reverse order.

		// First, gather all nodes into a flat list
		std::vector<asmjit::BaseNode*> nodes;
		for(auto* node = m_asm.lastNode(); node; node = node->prev())
			nodes.push_back(node);
		// nodes is now in reverse order (last to first) which is what we want for backward analysis

		// Track live registers. At basic block boundaries (labels, branches), we
		// conservatively assume all registers are live.
		std::set<RegKey> liveRegs;
		bool allLive = true;  // start conservative at the end of the function

		for(auto* node : nodes)
		{
			// At labels and control flow, reset to conservative (all live)
			if(node->isLabel() || isControlFlow(node))
			{
				allLive = true;
				liveRegs.clear();
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
				// This instruction has side effects; mark all read registers as live
				for(uint32_t i = 0; i < rwInfo.opCount(); ++i)
				{
					const auto& opInfo = rwInfo.operand(i);
					if(opInfo.isRead() || opInfo.isReadWrite())
					{
						RegKey key;
						if(getRegKey(key, inst->op(i)))
							liveRegs.insert(key);
					}

					// Handle memory operands: base and index registers are read
					if(inst->op(i).isMem())
					{
						const auto& mem = inst->op(i).as<asmjit::BaseMem>();
						if(mem.hasBaseReg())
						{
							RegKey key;
							key.group = static_cast<uint32_t>(asmjit::RegGroup::kGp);
							key.id = mem.baseId();
							liveRegs.insert(key);
						}
						if(mem.hasIndexReg())
						{
							RegKey key;
							key.group = static_cast<uint32_t>(asmjit::RegGroup::kGp);
							key.id = mem.indexId();
							liveRegs.insert(key);
						}
					}
				}

				// If instruction reads CPU flags, mark them as live
				if(rwInfo.readFlags() != asmjit::CpuRWFlags::kNone)
					liveRegs.insert(cpuFlagsKey());

				continue;
			}

			// Side-effect-free instruction: check if all written outputs are dead
			if(allLive)
				continue;  // conservative: can't eliminate if all regs might be live

			bool anyWrittenLive = false;
			std::vector<RegKey> writtenKeys;

			for(uint32_t i = 0; i < rwInfo.opCount(); ++i)
			{
				const auto& opInfo = rwInfo.operand(i);
				if(opInfo.isWrite() || opInfo.isReadWrite())
				{
					RegKey key;
					if(getRegKey(key, inst->op(i)))
					{
						writtenKeys.push_back(key);
						if(liveRegs.count(key))
							anyWrittenLive = true;
					}
				}
			}

			// Check if written CPU flags are live
			if(rwInfo.writeFlags() != asmjit::CpuRWFlags::kNone)
			{
				if(liveRegs.count(cpuFlagsKey()))
					anyWrittenLive = true;
			}

			if(!anyWrittenLive && !writtenKeys.empty())
			{
				// All outputs are dead, remove this instruction
				m_asm.removeNode(node);
				++removed;
				continue;
			}

			// Instruction is not dead. Update liveness:
			// Written registers become dead (removed from live set) unless also read
			for(uint32_t i = 0; i < rwInfo.opCount(); ++i)
			{
				const auto& opInfo = rwInfo.operand(i);
				RegKey key;
				if(!getRegKey(key, inst->op(i)))
					continue;

				if(opInfo.isWriteOnly())
					liveRegs.erase(key);

				if(opInfo.isRead() || opInfo.isReadWrite())
					liveRegs.insert(key);
			}

			// Handle memory operands: base/index registers are read
			for(uint32_t i = 0; i < inst->opCount(); ++i)
			{
				if(inst->op(i).isMem())
				{
					const auto& mem = inst->op(i).as<asmjit::BaseMem>();
					if(mem.hasBaseReg())
					{
						RegKey key;
						key.group = static_cast<uint32_t>(asmjit::RegGroup::kGp);
						key.id = mem.baseId();
						liveRegs.insert(key);
					}
					if(mem.hasIndexReg())
					{
						RegKey key;
						key.group = static_cast<uint32_t>(asmjit::RegGroup::kGp);
						key.id = mem.indexId();
						liveRegs.insert(key);
					}
				}
			}

			// CPU flags liveness
			if(rwInfo.writeFlags() != asmjit::CpuRWFlags::kNone)
				liveRegs.erase(cpuFlagsKey());
			if(rwInfo.readFlags() != asmjit::CpuRWFlags::kNone)
				liveRegs.insert(cpuFlagsKey());

		}

		return removed;
	}

	// Constant folding: track known constant values in registers and evaluate
	// arithmetic at compile time when all inputs are constants.
	size_t JitOptimizer::constantFolding()
	{
		const auto arch = asmjit::Environment::host().arch();
		size_t folded = 0;

		std::map<RegKey, int64_t> constants;
		std::map<uint32_t, std::map<RegKey, int64_t>> labelConstants;

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
							isConditional = true;  // has a label target = conditional branch
							const auto labelId = cfInst->op(i).as<asmjit::Label>().id();
							// Save current constants for the label target.
							// If multiple branches target the same label, intersect.
							auto it = labelConstants.find(labelId);
							if(it == labelConstants.end())
								labelConstants[labelId] = constants;
							else
							{
								// Intersect: keep only constants that match on both paths
								for(auto ci = it->second.begin(); ci != it->second.end();)
								{
									auto fi = constants.find(ci->first);
									if(fi == constants.end() || fi->second != ci->second)
										ci = it->second.erase(ci);
									else
										++ci;
								}
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
				// the saved branch-path constants.  Only keep constants valid
				// on ALL paths reaching this label.
				const auto labelId = node->as<asmjit::LabelNode>()->labelId();
				auto it = labelConstants.find(labelId);
				if(it != labelConstants.end())
				{
					// Intersect: keep only constants present in both sets with same value
					for(auto ci = constants.begin(); ci != constants.end();)
					{
						auto fi = it->second.find(ci->first);
						if(fi == it->second.end() || fi->second != ci->second)
							ci = constants.erase(ci);
						else
							++ci;
					}
					labelConstants.erase(it);
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

			const bool flagsAreLive = (rwInfo.writeFlags() != asmjit::CpuRWFlags::kNone) && areFlagsLive(node);

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
					RegKey key;
					if(getRegKey(key, inst->op(0)))
					{
						constants[key] = maskToRegSize(inst->op(0), inst->op(1).as<asmjit::Imm>().value());
					}
					node = next;
					continue;
				}

				// mov reg, reg -> propagate constant
				if(opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
				{
					RegKey srcKey;
					RegKey dstKey;
					if(getRegKey(dstKey, inst->op(0)) && getRegKey(srcKey, inst->op(1)))
					{
						auto it = constants.find(srcKey);
						if(it != constants.end())
							constants[dstKey] = maskToRegSize(inst->op(0), it->second);
						else
							constants.erase(dstKey);
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
					RegKey dstKey;
					if(getRegKey(dstKey, inst->op(0)))
					{
						auto it = constants.find(dstKey);
						if(it != constants.end())
						{
							const auto regVal = it->second;
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
								constants[dstKey] = maskToRegSize(inst->op(0), result);
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
					RegKey dstKey, srcKey;
					if(getRegKey(dstKey, inst->op(0)) && getRegKey(srcKey, inst->op(1)))
					{
						auto itDst = constants.find(dstKey);
						auto itSrc = constants.find(srcKey);
						if(itDst != constants.end() && itSrc != constants.end())
						{
							const auto dstVal = itDst->second;
							const auto srcVal = itSrc->second;
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
								constants[dstKey] = maskToRegSize(inst->op(0), result);
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
					RegKey srcKey;
					if(getRegKey(srcKey, inst->op(1)))
					{
						auto it = constants.find(srcKey);
						if(it != constants.end())
						{
							const auto srcVal = it->second;
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
								RegKey dstKey;
								if(getRegKey(dstKey, inst->op(0)))
								{
									inst->setId(Inst::kIdMov);
									inst->setOpCount(2);
									inst->setOp(0, inst->op(0));
									inst->setOp(1, asmjit::Imm(result));
									inst->resetOp(2);
									constants[dstKey] = maskToRegSize(inst->op(0), result);
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
				RegKey k0, k1;
				if(getRegKey(k0, inst->op(0)) && getRegKey(k1, inst->op(1)) && k0 == k1)
				{
					constants[k0] = 0;
					node = next;
					continue;
				}
			}

			// x86: neg reg -> negate constant
			if(!flagsAreLive && instId == Inst::kIdNeg && opCount == 1 && inst->op(0).isReg())
			{
				RegKey key;
				if(getRegKey(key, inst->op(0)))
				{
					auto it = constants.find(key);
					if(it != constants.end())
					{
						const auto result = -it->second;
						inst->setId(Inst::kIdMov);
						inst->setOpCount(2);
						inst->setOp(1, asmjit::Imm(result));
						constants[key] = maskToRegSize(inst->op(0), result);
						++folded;
						didFold = true;
					}
				}
			}

			// x86: not reg -> bitwise not constant
			if(!flagsAreLive && instId == Inst::kIdNot && opCount == 1 && inst->op(0).isReg())
			{
				RegKey key;
				if(getRegKey(key, inst->op(0)))
				{
					auto it = constants.find(key);
					if(it != constants.end())
					{
						const auto result = ~it->second;
						inst->setId(Inst::kIdMov);
						inst->setOpCount(2);
						inst->setOp(1, asmjit::Imm(result));
						constants[key] = maskToRegSize(inst->op(0), result);
						++folded;
						didFold = true;
					}
				}
			}
#else
			// ARM64: neg reg, reg -> negate constant
			if(instId == Inst::kIdNeg && opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
			{
				RegKey srcKey;
				if(getRegKey(srcKey, inst->op(1)))
				{
					auto it = constants.find(srcKey);
					if(it != constants.end())
					{
						RegKey dstKey;
						if(getRegKey(dstKey, inst->op(0)))
						{
							const auto result = -it->second;
							inst->setId(Inst::kIdMov);
							inst->setOp(1, asmjit::Imm(result));
							constants[dstKey] = maskToRegSize(inst->op(0), result);
							++folded;
							didFold = true;
						}
					}
				}
			}

			// ARM64: mvn reg, reg -> bitwise not constant
			if(instId == Inst::kIdMvn && opCount == 2 && inst->op(0).isReg() && inst->op(1).isReg())
			{
				RegKey srcKey;
				if(getRegKey(srcKey, inst->op(1)))
				{
					auto it = constants.find(srcKey);
					if(it != constants.end())
					{
						RegKey dstKey;
						if(getRegKey(dstKey, inst->op(0)))
						{
							const auto result = ~it->second;
							inst->setId(Inst::kIdMov);
							inst->setOp(1, asmjit::Imm(result));
							constants[dstKey] = maskToRegSize(inst->op(0), result);
							++folded;
							didFold = true;
						}
					}
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
					RegKey src1Key;
					if(getRegKey(src1Key, inst->op(1)))
					{
						auto itSrc1 = constants.find(src1Key);
						if(itSrc1 != constants.end())
						{
							const auto src1Val = itSrc1->second;
							int64_t src2Val = 0;
							bool src2Known = false;

							if(inst->op(2).isImm())
							{
								src2Val = inst->op(2).as<asmjit::Imm>().value();
								src2Known = true;
							}
							else if(inst->op(2).isReg())
							{
								RegKey src2Key;
								if(getRegKey(src2Key, inst->op(2)))
								{
									auto itSrc2 = constants.find(src2Key);
									if(itSrc2 != constants.end())
									{
										src2Val = itSrc2->second;
										src2Known = true;
									}
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
									RegKey dstKey;
									if(getRegKey(dstKey, inst->op(0)))
									{
										inst->setId(Inst::kIdMov);
										inst->setOpCount(2);
										inst->setOp(1, asmjit::Imm(result));
										inst->resetOp(2);
										constants[dstKey] = maskToRegSize(inst->op(0), result);
										++folded;
										didFold = true;
									}
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
						RegKey key;
						if(getRegKey(key, inst->op(i)))
							constants.erase(key);
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
}
