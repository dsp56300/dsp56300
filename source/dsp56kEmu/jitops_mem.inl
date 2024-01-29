#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "opcodes.h"
#include "opcodetypes.h"
#include "peripherals.h"
#include "registers.h"

namespace dsp56k
{
	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> JitOps::EffectiveAddressType JitOps::effectiveAddressType(const TWord _op) const
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(_op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(_op);

		if ((mmm << 3 | rrr) == MMMRRR_ImmediateData)
			return Immediate;

		if(mmm == MMM_AbsAddr)
			return m_opWordB >= XIO_Reserved_High_First ? Peripherals : Memory;

		return Dynamic;
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> DspValue JitOps::effectiveAddress(const TWord _op)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(_op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(_op);

		return updateAddressRegister(mmm, rrr);
	}

	template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> void JitOps::readMem(DspValue& _dst, const TWord _op)
	{
		readMem<Inst>(_dst, _op, getFieldValueMemArea<Inst>(_op));
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> JitOps::EffectiveAddressType JitOps::readMem(DspValue& _dst, const TWord _op, const EMemArea _area)
	{
		const auto eaType = effectiveAddressType<Inst>(_op);

		switch (eaType)
		{
		case Immediate:
			getOpWordB(_dst);
			break;
		case Memory:
			m_block.mem().readDspMemory(_dst, _area, getOpWordB());
			break;
		case Peripherals:
			m_block.mem().readPeriph(_dst, _area, getOpWordB(), Inst);
			break;
		case Dynamic:
			const auto ea = effectiveAddress<Inst>(_op);
			readMemOrPeriph(_dst, _area, ea, Inst);
			break;
		}
		return eaType;
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> void JitOps::readMem(DspValue& _dst, TWord op) const
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_qqqqqq>(op);
		m_block.mem().readPeriph(_dst, area, offset + 0xffff80, Inst);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type*> void JitOps::readMem(DspValue& _dst, TWord op) const
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_pppppp>(op);
		m_block.mem().readPeriph(_dst, area, offset + 0xffffc0, Inst);
	}
	template <Instruction Inst, typename std::enable_if<!hasField<Inst, Field_s>() && hasFields<Inst, Field_aaaaaa, Field_S>()>::type*> void JitOps::readMem(DspValue& _dst, TWord op) const
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_aaaaaa>(op);
		m_block.mem().readDspMemory(_dst, area, offset);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_S, Field_s>() && hasField<Inst, Field_aaaaaa>()>::type*> void JitOps::readMem(DspValue& _dst, TWord op, EMemArea _area) const
	{
		const auto offset = getFieldValue<Inst,Field_aaaaaa>(op);
		m_block.mem().readDspMemory(_dst, _area, offset);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> void JitOps::writeMem(TWord op, const DspValue& _src)
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_qqqqqq>(op);
		m_block.mem().writePeriph(area, static_cast<TWord>(offset + 0xffff80), _src);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type*> void JitOps::writeMem(TWord op, const DspValue& _src)
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_pppppp>(op);
		m_block.mem().writePeriph(area, static_cast<TWord>(offset + 0xffffc0), _src);
	}

	template <Instruction Inst, std::enable_if_t<(!hasFields<Inst,Field_s, Field_S>() || Inst==Movep_ppea) && hasFields<Inst, Field_MMM, Field_RRR>()>*> JitOps::EffectiveAddressType JitOps::writeMem(const TWord _op, const EMemArea _area, DspValue& _src)
	{
		const auto eaType = effectiveAddressType<Inst>(_op);

		switch (eaType)
		{
		case Immediate:
			assert(0 && "unable to write to immediate data");
			break;
		case Memory:
			m_block.mem().writeDspMemory(_area, getOpWordB(), _src);
			break;
		case Peripherals:
			m_block.mem().writePeriph(_area, getOpWordB(), _src);
			break;
		case Dynamic:
			{
				// things such as move r5,x:(r5)+ will have r5 in src while trying to update r5 in effectiveAddress<> before we write it. In this case, we need to copy r5 before it gets modified
				const auto rIndex = getFieldValue<Inst, Field_RRR>(_op);
				if (_src.isDspReg(static_cast<PoolReg>(PoolReg::DspR0 + rIndex)))
					_src.toTemp();
				const auto ea = effectiveAddress<Inst>(_op);
				writeMemOrPeriph(_area, ea, _src);				
			}
			break;
		}
		return eaType;
	}

	template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> void JitOps::writeMem(const TWord _op, DspValue& _src)
	{
		const auto area = getFieldValueMemArea<Inst>(_op);
		writeMem<Inst>(_op, area, _src);
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_S, Field_s>() && hasField<Inst, Field_aaaaaa>()>::type*> void JitOps::writeMem(TWord op, EMemArea _area, const DspValue& _src) const
	{
		const auto offset = getFieldValue<Inst,Field_aaaaaa>(op);
		m_block.mem().writeDspMemory(_area, offset, _src);
	}

	template <Instruction Inst> void JitOps::writePmem(const TWord _op, const DspValue& _src)
	{
		auto ea = effectiveAddress<Inst>(_op);

		DspValue compare(m_block, UsePooledTemp);

		Jitmem::ScratchPMem pmem(m_block, false, true);
		m_block.mem().readDspMemory(compare, MemArea_P, ea, pmem);

		const auto skip = m_asm.newLabel();
		m_asm.cmp(r32(compare), r32(_src));
		m_asm.jz(skip);

		m_block.mem().writeDspMemory(MemArea_P, ea, _src, pmem);

		m_block.mem().mov(m_block.pMemWriteAddress(), ea);
		m_block.mem().mov(m_block.pMemWriteValue(), _src);

		m_asm.bind(skip);

		m_resultFlags |= WritePMem;
	}
}
