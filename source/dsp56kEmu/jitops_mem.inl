#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "jitregtypes.h"
#include "opcodes.h"
#include "opcodetypes.h"
#include "peripherals.h"
#include "registers.h"

#include "asmjit/core/operand.h"

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

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> JitOps::EffectiveAddressType JitOps::effectiveAddress(const JitReg64& _dst, const TWord _op)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(_op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(_op);

		updateAddressRegister(_dst, mmm, rrr);

		return effectiveAddressType<Inst>(_op);
	}

	template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> void JitOps::readMem(const JitReg64& _dst, const TWord _op)
	{
		readMem<Inst>(_dst, _op, getFieldValueMemArea<Inst>(_op));
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> JitOps::EffectiveAddressType JitOps::readMem(const JitReg64& _dst, const TWord _op, const EMemArea _area)
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
			m_block.mem().readPeriph(_dst, _area, getOpWordB());
			break;
		case Dynamic:
			effectiveAddress<Inst>(_dst, _op);
			readMemOrPeriph(_dst, _area, _dst);
			break;
		}
		return eaType;
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> void JitOps::readMem(const JitReg64& _dst, TWord op) const
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_qqqqqq>(op);
		m_asm.mov(_dst, asmjit::Imm(offset + 0xffff80));
		m_block.mem().readPeriph(_dst, area, _dst);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type*> void JitOps::readMem(const JitReg64& _dst, TWord op) const
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_pppppp>(op);
		m_asm.mov(_dst, asmjit::Imm(offset + 0xffffc0));
		m_block.mem().readPeriph(_dst, area, _dst);	
	}
	template <Instruction Inst, typename std::enable_if<!hasField<Inst, Field_s>() && hasFields<Inst, Field_aaaaaa, Field_S>()>::type*> void JitOps::readMem(const JitReg64& _dst, TWord op) const
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_aaaaaa>(op);
		m_block.mem().readDspMemory(_dst, area, offset);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_S, Field_s>() && hasField<Inst, Field_aaaaaa>()>::type*> void JitOps::readMem(const JitReg64& _dst, TWord op, EMemArea _area) const
	{
		const auto offset = getFieldValue<Inst,Field_aaaaaa>(op);
		m_block.mem().readDspMemory(_dst, _area, offset);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> void JitOps::writeMem(TWord op, const JitReg64& _src)
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_qqqqqq>(op);
		m_block.mem().writePeriph(area, static_cast<TWord>(offset + 0xffff80), _src);
	}
	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type*> void JitOps::writeMem(TWord op, const JitReg64& _src)
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto offset = getFieldValue<Inst,Field_pppppp>(op);
		m_block.mem().writePeriph(area, static_cast<TWord>(offset + 0xffffc0), _src);
	}

	template <Instruction Inst, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type*> JitOps::EffectiveAddressType JitOps::writeMem(const TWord _op, const EMemArea _area, const JitReg64& _src)
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
				const RegGP offset(m_block);
				effectiveAddress<Inst>(offset, _op);
				writeMemOrPeriph(_area, offset, _src);				
			}
			break;
		}
		return eaType;
	}

	template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> void JitOps::writeMem(const TWord _op, const JitReg64& _src)
	{
		const auto area = getFieldValueMemArea<Inst>(_op);
		writeMem<Inst>(_op, area, _src);
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_S, Field_s>() && hasField<Inst, Field_aaaaaa>()>::type*> void JitOps::writeMem(TWord op, EMemArea _area, const JitReg64& _src) const
	{
		const auto offset = getFieldValue<Inst,Field_aaaaaa>(op);
		m_block.mem().writeDspMemory(_area, offset, _src);
	}

	void JitOps::readMemOrPeriph(const JitReg64& _dst, EMemArea _area, const JitReg64& _offset)
	{
		if(_area == MemArea_P)
		{
			m_block.mem().readDspMemory(_dst, MemArea_P, _offset);
			return;
		}

		// I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing so we're lost here
		const auto readPeriph = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.cmp(_offset, asmjit::Imm(XIO_Reserved_High_First));

		m_asm.jge(readPeriph);

		m_block.mem().readDspMemory(_dst, _area, _offset);
		m_asm.jmp(end);

		m_asm.bind(readPeriph);
		m_block.mem().readPeriph(_dst, _area, _offset);
		m_asm.bind(end);		
	}
	void JitOps::writeMemOrPeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value)
	{
		if(_area == MemArea_P)
		{
			m_block.mem().writeDspMemory(MemArea_P, _offset, _value);
			return;
		}

		// I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing so we're lost here
		const auto readPeriph = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.cmp(_offset, asmjit::Imm(XIO_Reserved_High_First));

		m_asm.jge(readPeriph);

		m_block.mem().writeDspMemory(_area, _offset, _value);
		m_asm.jmp(end);

		m_asm.bind(readPeriph);
		m_block.mem().writePeriph(_area, _offset, _value);
		m_asm.bind(end);		
	}
}
