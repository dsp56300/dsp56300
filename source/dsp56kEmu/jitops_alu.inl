#pragma once

#include "jitops.h"
#include "jitops_mem.inl"

#include "dsp_decode.inl"

namespace dsp56k
{
	template<Instruction Inst> void JitOps::bitmod_ea(TWord op, void( JitOps::*_bitmodFunc)(const DspValue&, TWord))
	{
		const auto area = getFieldValueMemArea<Inst>(op);

		auto eaType = effectiveAddressType<Inst>(op);

		// not sure if this can happen, iirc I've seen this once. Handle it
		if(eaType == Immediate)
			eaType = m_opWordB >= XIO_Reserved_High_First ? Peripherals : Memory;

		DspValue regMem(m_block);

		switch (eaType)
		{
		case Peripherals:
			{
				const TWord offset = getOpWordB();
				m_block.mem().readPeriph(regMem, area, offset, Inst);
				(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
				m_block.mem().writePeriph(area, offset, regMem);
			}
			break;
		case Memory:
			{
				const TWord offset = getOpWordB();
				bitmod_aa<Inst>(op, offset, _bitmodFunc);
			}
			break;
		case Dynamic:
			{
				const auto ea = effectiveAddress<Inst>(op);			
				readMemOrPeriph(regMem, area, ea, Inst);
				(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
				writeMemOrPeriph(area, ea, regMem);
			}
			break;
		}
	}
	
	template<Instruction Inst> void JitOps::bitmod_aa(TWord op, void( JitOps::*_bitmodFunc)(const DspValue&, TWord))
	{
		const auto addr = getFieldValue<Inst, Field_aaaaaa>(op);
		bitmod_aa<Inst>(op, addr, _bitmodFunc);
	}

	template<Instruction Inst> void JitOps::bitmod_aa(TWord op, TWord addr, void( JitOps::*_bitmodFunc)(const DspValue&, TWord))
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		DspValue regMem(m_block);
		regMem.temp(DspValue::Memory);
		Jitmem::ScratchPMem scratch(m_block);
		const auto p = m_block.mem().getMemAreaPtr(scratch, area, addr);
		m_block.mem().readDspMemory(regMem, p);
		(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
		m_block.mem().writeDspMemory(p, regMem);
	}

	template<Instruction Inst> void JitOps::bitmod_ppqq(TWord op, void( JitOps::*_bitmodFunc)(const DspValue&, TWord))
	{
		DspValue r(m_block);
		readMem<Inst>(r, op);
		(this->*_bitmodFunc)(r, getBit<Inst>(op));
		writeMem<Inst>(op, r);
	}

	template<Instruction Inst> void JitOps::bitmod_D(TWord op, void( JitOps::*_bitmodFunc)(const DspValue&, TWord))
	{
		const auto bit		= getBit<Inst>(op);
		const auto dddddd	= getFieldValue<Inst,Field_DDDDDD>(op);

		DspValue d(m_block);
		decode_dddddd_read(d, dddddd);
		(this->*_bitmodFunc)(d, getBit<Inst>(op));
		decode_dddddd_write(dddddd, d);
	}

	template<Instruction Inst, bool Accumulate, bool Round> void JitOps::op_Mac_S(TWord op)
	{
		const auto sssss	= getFieldValue<Inst,Field_sssss>(op);
		const auto qq		= getFieldValue<Inst,Field_QQ>(op);
		const auto ab		= getFieldValue<Inst,Field_d>(op);
		const auto negate	= getFieldValue<Inst,Field_k>(op);

		DspValue s1(m_block);
		decode_QQ_read(s1, qq, true);

		DspValue s2(m_block, DSP::decode_sssss(sssss), DspValue::Immediate24);

		alu_mpy(ab, s1, s2, negate, Accumulate, false, false, Round);
	}

	template<Instruction Inst, bool Accumulate> void JitOps::op_Mpy_su(TWord op)
	{
		const bool ab		= getFieldValue<Inst,Field_d>(op);
		const bool negate	= getFieldValue<Inst,Field_k>(op);
		const bool uu		= getFieldValue<Inst,Field_s>(op);
		const TWord qqqq	= getFieldValue<Inst,Field_QQQQ>(op);

		DspValue s1(m_block);
		DspValue s2(m_block);
		decode_QQQQ_read( s1, !uu, s2, false, qqqq);

		alu_mpy(ab, s1, s2, negate, Accumulate, uu, true, false);
	}
}
