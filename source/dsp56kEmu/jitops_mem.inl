#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "jittypes.h"
#include "opcodes.h"
#include "opcodetypes.h"
#include "peripherals.h"
#include "registers.h"

#include "asmjit/core/operand.h"

namespace dsp56k
{
	template <Instruction Inst, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type*> void JitOps::readMem(const JitReg64& _dst, const TWord _op, const EMemArea _area)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(_op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(_op);

		if ((mmm << 3 | rrr) == MMMRRR_ImmediateData)
		{
			getOpWordB(_dst);
			return;
		}

		updateAddressRegister(_dst, mmm, rrr);

		// I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing so we're lost here
		const auto readPeriph = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.cmp(_dst, asmjit::Imm(XIO_Reserved_High_First));

		m_asm.jge(readPeriph);

		m_block.mem().readDspMemory(_dst, _area, _dst);
		m_asm.jmp(end);

		m_asm.bind(readPeriph);
		m_block.mem().readPeriph(_dst, _area, _dst);
		m_asm.bind(end);
	}
	
	template <Instruction Inst, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type*> void JitOps::writeMem(const TWord _op, const EMemArea _area, const JitReg64& _src)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(_op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(_op);

		if ((mmm << 3 | rrr) == MMMRRR_ImmediateData)
		{
			assert(0 && "unable to write to immediate data");
			return;
		}

		const RegGP offset(m_block);
		updateAddressRegister(offset, mmm, rrr);

		// I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing so we're lost here
		const auto readPeriph = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.cmp(offset, asmjit::Imm(XIO_Reserved_High_First));

		m_asm.jge(readPeriph);

		m_block.mem().writeDspMemory(_area, offset, _src);
		m_asm.jmp(end);

		m_asm.bind(readPeriph);
		m_block.mem().writePeriph(_area, offset, _src);
		m_asm.bind(end);
	}
}
