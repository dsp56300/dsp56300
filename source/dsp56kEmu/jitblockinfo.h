#pragma once

#include "opcodeanalysis.h"
#include "types.h"

namespace dsp56k
{
	struct JitBlockInfo
	{
		enum class TerminationReason
		{
			None,
			ExistingCode,
			PcMax,
			VolatileP,
			WriteLoopRegs,
			Branch,
			PopPC,
			LoopBegin,
			WritePMem,
			LoopEnd,
			InstructionLimit,
			ModeChange
		};

		enum class Flags
		{
			None				= 0x00,
			WritesSRbeforeRead	= 0x01,
			ModeChange			= 0x02,
			IsLoopBodyBegin		= 0x04,
		};

		auto hasFlag(const Flags _flag) const
		{
			return flags & static_cast<uint32_t>(_flag);
		}

		void addFlag(const Flags _flag)
		{
			flags |= static_cast<uint32_t>(_flag);
		}

		TerminationReason terminationReason = TerminationReason::None;
		uint32_t flags = 0;

		TWord pc = 0;
		TWord memSize = 0;
		TWord instructionCount = 0;
		RegisterMask readRegs = RegisterMask::None;
		RegisterMask writtenRegs = RegisterMask::None;
		TWord branchTarget = g_invalidAddress;
		bool branchIsConditional = false;
		TWord loopBegin = g_invalidAddress;
		TWord loopEnd = g_invalidAddress;
	};
}
