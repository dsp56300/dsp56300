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
			eaType = isCppHandledAddress(m_opWordB) ? Peripherals : Memory;

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
		auto mr = m_block.mem().readDspMemory(regMem, area, addr);
		(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
		m_block.mem().writeDspMemory(area, addr, regMem, std::move(mr));
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

		// workaround for an undocumented DSP feature, a bug in a code we've seen. It uses
		// bclr #22,b
		// This is not supposed to work according to the documentation, but it does.
		// The DSP transfers the alu to a 24 bit reg, modifies, then writes it back
		// That is why we prevent to use a reference to the ALU directly here
		if(dddddd == 0x0e || dddddd == 0x0f)
		{
			DspValue d(m_block);
			decode_dddddd_read(d, dddddd);
			(this->*_bitmodFunc)(d, getBit<Inst>(op));
			decode_dddddd_write(dddddd, d);
			return;
		}

		// SR needs its own read-modify-write, for two independent reasons. Its condition codes are
		// evaluated lazily, and only decode_dddddd_read/_write (getSR/setSR) flush a pending update
		// and declare the register written - through a plain reference the pending update overwrote
		// the bit again, so a "bchg #$3,sr" in front of a bge silently did nothing.
		//
		// The copy is not redundant: getSR hands back a REFERENCE to the pooled SR, so modifying it
		// in place would let the C write these instructions perform - C takes the tested bit - stand.
		// On hardware it does not: the modified value is written back wholesale, bit 0 included, so
		// C ends up as bit 0 of the result. Measured on the reference simulator, e.g.
		// bchg #$3,sr on $c00308 gives $c00300 and not $c00301.
		if(dddddd == 0x39)
		{
			DspValue d(m_block);
			d.temp(DspValue::Temp24);
			{
				DspValue sr(m_block);
				decode_dddddd_read(sr, dddddd);
				m_asm.mov(r32(d), r32(sr));
			}

			(this->*_bitmodFunc)(d, bit);

			decode_dddddd_write(dddddd, d);
			return;
		}

		auto dRead = decode_dddddd_ref(dddddd, true, false);
		if(!dRead.isRegValid())
			decode_dddddd_read(dRead, dddddd);

		(this->*_bitmodFunc)(dRead, getBit<Inst>(op));

		auto dWrite = decode_dddddd_ref(dddddd, true, true);
		if(!dWrite.isType(DspValue::DspReg24))
			decode_dddddd_write(dddddd, dRead);
	}

	template<Instruction Inst, bool Accumulate, bool Round> void JitOps::op_Mac_S(TWord op)
	{
		const auto sssss	= getFieldValue<Inst,Field_sssss>(op);
		const auto qq		= getFieldValue<Inst,Field_QQ>(op);
		const auto ab		= getFieldValue<Inst,Field_d>(op);
		const auto negate	= getFieldValue<Inst,Field_k>(op);

		DspValue s1(m_block);
		decode_QQ_read(s1, qq, true, g_mpyOperandShift);

		DspValue s2(m_block, DSP::decode_sssss(sssss), DspValue::Immediate24);

		alu_mpy(ab, s1, s2, negate, Accumulate, false, false, Round, g_mpyOperandShift);
	}

	template<Instruction Inst, bool Accumulate> void JitOps::op_Mpy_su(TWord op)
	{
		const bool ab		= getFieldValue<Inst,Field_d>(op);
		const bool negate	= getFieldValue<Inst,Field_k>(op);
		const bool uu		= getFieldValue<Inst,Field_s>(op);
		const TWord qqqq	= getFieldValue<Inst,Field_QQQQ>(op);

		DspValue s1(m_block);
		DspValue s2(m_block);

		// su sign extends s1, so the product scale rides along in that shift for free. uu does
		// not - s1 is a bare 32 bit mov there - so pre-scaling it would only move the shift
		// around, and it stays out of the operand.
		const auto s1Shift = uu ? 0u : g_mpyOperandShift;

		decode_QQQQ_read( s1, !uu, s2, false, qqqq, s1Shift);

		alu_mpy(ab, s1, s2, negate, Accumulate, uu, true, false, s1Shift);
	}
}
