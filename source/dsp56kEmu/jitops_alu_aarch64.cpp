#include "jittypes.h"

#ifdef HAVE_ARM64

#include "opcodecycles.h"
#include "dsp.h"
#include "jitblockruntimedata.h"
#include "jitdspmode.h"
#include "jitops.h"
#include "jitops_mem.inl"
#include "asmjit/core/operand.h"

// Unfortunaly headers for Windows ARM define mvn as an unscoped macro, we need to get rid of it as its an aarch64 instruction and we use it via asmjit
#undef mvn

namespace dsp56k
{
	void JitOps::XY0to56(const JitReg64& _dst, int _xy) const
	{
		getBlock().dspRegPool().getXY0(r32(_dst), _xy);

		m_asm.sbfiz(_dst, _dst, asmjit::Imm(32), asmjit::Imm(24));
		m_asm.lsr(_dst, _dst, asmjit::Imm(8));
	}

	void JitOps::XY1to56(const JitReg64& _dst, int _xy) const
	{
		getBlock().dspRegPool().getXY1(r32(_dst), _xy);

		m_asm.sbfiz(_dst, _dst, asmjit::Imm(32), asmjit::Imm(24));
		m_asm.lsr(_dst, _dst, asmjit::Imm(8));
	}

	void JitOps::alu_abs(const JitRegGP& _r)
	{
		m_asm.cmp(_r, asmjit::Imm(0));
		m_asm.cneg(_r, _r, asmjit::arm::CondCode::kLT);			// negate if < 0
	}
	
	void JitOps::alu_and(const TWord ab, DspValue& _v)
	{
		m_asm.shl(r64(_v), asmjit::Imm(24));

		AluRef alu(m_block, ab);

		{
			const RegGP r(m_block);
			m_asm.ands(r, alu.get(), r64(_v));
			ccr_update_ifZero(CCRB_Z);

			m_asm.lsr(r, r, asmjit::Imm(24));
			m_asm.bfi(alu, r, asmjit::Imm(24), asmjit::Imm(24));
		}

		_v.release();

		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(alu);
		ccr_clear(CCR_V);
	}

	void JitOps::alu_asl(const TWord _abSrc, const TWord _abDst, const ShiftReg* _v, TWord _immediate/* = 0*/)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_dspRegs.getALU(alu.get(), _abSrc);

		signextend56to64(alu);

		const RegGP oldAlu(m_block);
		m_asm.mov(oldAlu, alu);

		if(_v)
			m_asm.lsl(alu, alu, _v->get());
		else
			m_asm.lsl(alu, alu, asmjit::Imm(_immediate));

		// carry is the last bit shifted out so in our case its 56
		copyBitToCCR(alu, 56, CCRB_C);

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical ot the backshifted one
		{
			const RegScratch s(m_block);
			signextend56to64(s, alu);
			if(_v)
				m_asm.asr(s, s, _v->get());
			else
				m_asm.asr(s, s, asmjit::Imm(_immediate));
			m_asm.cmp(s, oldAlu.get());
		}

		ccr_update_ifNotZero(CCRB_V);

		m_dspRegs.mask56(alu);

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}
	
	void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg* _v, TWord _immediate/* = 0*/)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_dspRegs.getALU(alu.get(), _abSrc);

		const CcrBatchUpdate bu(*this, CCR_C, CCR_V);

		m_asm.lsl(alu, alu, asmjit::Imm(8));	// make sign-extend possible in our wide registers
		if(_v)
			m_asm.asr(alu, alu, _v->get());
		else
			m_asm.asr(alu, alu, asmjit::Imm(_immediate));

		copyBitToCCR(alu, 7, CCRB_C);			// carry is the last bit shifted out, we can grab it at bit pos 7 now as we pre-shifted left by 8

		m_asm.lsr(alu, alu, asmjit::Imm(8));	// correction

//		ccr_clear(CCR_V);						// cleared by batch update

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_bclr(const DspValue& _dst, const TWord _bit)
	{
		copyBitToCCR(_dst.get(), _bit, CCRB_C);
		m_asm.and_(_dst.get(), _dst.get(), asmjit::Imm(~(1ull << _bit)));
	}

	void JitOps::alu_bset(const DspValue& _dst, const TWord _bit)
	{
		copyBitToCCR(_dst.get(), _bit, CCRB_C);
		m_asm.orr(_dst.get(), _dst.get(), asmjit::Imm(1ull << _bit));
	}

	void JitOps::alu_bchg(const DspValue& _dst, const TWord _bit)
	{
		copyBitToCCR(_dst.get(), _bit, CCRB_C);
		m_asm.eor(_dst.get(), _dst.get(), asmjit::Imm(1ull << _bit));
	}

	void JitOps::alu_lsl(TWord ab, const DspValue& _shiftAmount)
	{
		DspValue d(m_block);
		getALU1(d, ab);

		if(_shiftAmount.isImm24())
			m_asm.lsl(r32(d), r32(d), asmjit::Imm(_shiftAmount.imm24()));
		else
			m_asm.lsl(r32(d), r32(d), _shiftAmount.get());

		copyBitToCCR(r32(d), 24, CCRB_C);

		m_asm.ands(r32(d), r32(d), asmjit::Imm(0xffffff));
		ccr_update_ifZero(CCRB_Z);

		copyBitToCCR(r32(d.get()), 23, CCRB_N);
		ccr_clear(CCR_V);

		setALU1(ab, d);
	}

	void JitOps::alu_lsr(TWord ab, const DspValue& _shiftAmount)
	{
		DspValue d(m_block);
		getALU1(d, ab);
		m_asm.lsl(r32(d), r32(d), asmjit::Imm(1));		// we need to preseve the carry bit to be able to copy it
		if(_shiftAmount.isImm24())
			m_asm.lsr(r32(d), r32(d), asmjit::Imm(_shiftAmount.imm24()));
		else
			m_asm.lsr(r32(d), r32(d), _shiftAmount.get());

		copyBitToCCR(r32(d), 0, CCRB_C);
		m_asm.lsr(r32(d), r32(d), asmjit::Imm(1));

		m_asm.test_(r32(d.get()));
		ccr_update_ifZero(CCRB_Z);
		copyBitToCCR(r32(d), 23, CCRB_N);

		ccr_clear(CCR_V);
		setALU1(ab, d);
	}

	void JitOps::alu_rnd(TWord ab, const JitReg64& d, const bool _needsSignextend/* = true*/)
	{
		if(_needsSignextend)
			signextend56to64(d);

		RegGP rounder(m_block);

		const JitDspMode* mode = m_block.getMode();

		if(mode)
		{
			uint32_t r = 0x800000;

			if(mode->testSR(SRB_S1))	r >>= 1;
			if(mode->testSR(SRB_S0))	r <<= 1;

			m_asm.mov(rounder, asmjit::Imm(r));
		}
		else
		{
			m_asm.mov(rounder, asmjit::Imm(0x800000));

			const ShiftReg shifter(m_block);
			m_asm.mov(shifter, asmjit::a64::xzr);
			sr_getBitValue(shifter, SRB_S1);
			m_asm.shr(rounder, shifter.get());
			sr_getBitValue(shifter, SRB_S0);
			m_asm.shl(rounder, shifter.get());
		}

		m_asm.add(d, rounder.get());

		m_asm.shl(rounder, asmjit::Imm(1));

		{
			// mask = all the bits to the right of, and including the rounding position
			const RegGP mask(m_block);
			m_asm.sub(mask, rounder, asmjit::Imm(1));

			auto noSM = [&]()
			{
				// convergent rounding. If all mask bits are cleared

				// then the bit to the left of the rounding position is cleared in the result
				// if (!(_alu.var & mask)) 
				//	_alu.var&=~(rounder<<1);

				{
					const RegScratch aluIfAndWithMaskIsZero(m_block);
					m_asm.bic(aluIfAndWithMaskIsZero, d, rounder.get());
					m_asm.tst(d, mask.get());
					m_asm.csel(d, aluIfAndWithMaskIsZero.get(), d, asmjit::arm::CondCode::kZero);
				}
			};

			if(mode)
			{
				if(!mode->testSR(SRB_RM))
					noSM();
			}
			else
			{
				const auto skipNoScalingMode = m_asm.newLabel();

				// if (!sr_test_noCache(SR_RM))
				m_asm.tbnz(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_RM), skipNoScalingMode);
				noSM();
				m_asm.bind(skipNoScalingMode);
			}

			// all bits to the right of and including the rounding position are cleared.
			// _alu.var&=~mask;
			m_asm.bic(d, d, mask.get());
		}

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z | CCR_V));
		m_dspRegs.mask56(d);
	}


	void JitOps::alu_insert(TWord ab, const DspValue& _src, DspValue& _widthOffset)
	{
		AluRef d(m_block, ab);

		// const auto width = (widthOffset >> 12) & 0x3f;
		const ShiftReg width(m_block);
		_widthOffset.copyTo(width.get(), 24);
		m_asm.shr(width, asmjit::Imm(12));
		m_asm.and_(width, asmjit::Imm(0x3f));

		// const auto mask = (1<<width) - 1;
		const RegGP mask(m_block);
		m_asm.mov(r32(mask), asmjit::Imm(1));
		m_asm.shl(mask, width.get());
		m_asm.dec(mask);

		// const uint64_t offset = widthOffset & 0x3f;
		const auto& offset = width;
		m_asm.mov(r32(offset), r32(_widthOffset.get()));
		m_asm.and_(offset.get(), asmjit::Imm(0x3f));

		// uint64_t s = src & mask;
		const RegGP s(m_block);
		m_asm.mov(r32(s), r32(_src.get()));
		m_asm.and_(r32(s), r32(mask));

		// s <<= offset;
		m_asm.shl(s.get(), offset.get());

		// d &= ~(static_cast<uint64_t>(mask) << offset);
		m_asm.shl(r64(mask), offset.get());
		m_asm.bic(d.get(), d.get(), mask);

		// d |= s;
		m_asm.or_(d, s);

		ccr_clear(CCR_C);
		ccr_clear(CCR_V);
		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_U | CCR_N | CCR_Z));
	}

	void JitOps::op_Btst_ea(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_ea>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_ea>(op), CCRB_C);
	}

	void JitOps::op_Btst_aa(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_aa>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_aa>(op), CCRB_C);
	}

	void JitOps::op_Btst_pp(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_pp>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_pp>(op), CCRB_C);
	}

	void JitOps::op_Btst_qq(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_qq>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_qq>(op), CCRB_C);
	}

	void JitOps::op_Btst_D(TWord op)
	{
		const auto dddddd = getFieldValue<Btst_D, Field_DDDDDD>(op);
		const auto bit = getBit<Btst_D>(op);

		DspValue r(m_block);
		decode_dddddd_read(r, dddddd);

		copyBitToCCR(r.get(), bit, CCRB_C);
	}

	void JitOps::op_Clb(TWord op)
	{
		const auto S = getFieldValue(Clb, Field_S, op);
		const auto D = getFieldValue(Clb, Field_D, op);

		AluRef s(m_block, S, true, D == S);
		AluRef d(m_block, D, S == D, true);

		const RegGP t(m_block);
		const RegGP shifted(m_block);
		m_asm.lsl(r64(shifted), r64(s), asmjit::Imm(8));

		// this instruction counts the number of equal bits starting at the MSB
		// We only count leading zeroes, so we invert the source if the MSB is a 1
		m_asm.bitTest(r64(shifted), 63);
		m_asm.cinv(r64(t), r64(shifted), asmjit::arm::CondCode::kNotZero);

		// prevent that we get a result that is > 56
		m_asm.or_(r64(t), asmjit::Imm(0xff));

		m_asm.clz(r64(t), r64(t));
		m_asm.neg(r32(t));
		m_asm.add(r32(t), asmjit::Imm(9));	// range of DSP result is -47 ... +8

		// special case: if the source alu is 0, the result is 0
		m_asm.test_(s);
		m_asm.csel(r32(t), r32(t), asmjit::a64::regs::wzr, asmjit::arm::CondCode::kNotZero);

		CcrBatchUpdate ccrBatch(*this, CCR_N, CCR_Z, CCR_V);
		copyBitToCCR(d, 23, CCRB_N);

		m_asm.lsl(r64(d), r64(t), asmjit::Imm(24));
		ccr_update_ifZero(CCRB_Z);
	}

	void JitOps::op_Div(TWord op)
	{
		const auto ab = getFieldValue<Div, Field_d>(op);
		const auto jj = getFieldValue<Div, Field_JJ>(op);

		m_ccrRead |= CCR_C;
		updateDirtyCCR(CCR_C);

		AluRef d(m_block, ab);

		if (m_repMode == RepNone || m_repMode == RepLast)
		{
			// V and L updates
			// V: Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
			// L: Set if the Overflow bit (V) is set.
			// What we do is we check if bits 55 and 54 of the ALU are not identical (host parity bit cleared) and set V accordingly.
			// Nearly identical for L but L is only set, not cleared as it is sticky
			const RegGP r(m_block);
			const auto sr = m_dspRegs.getSR(JitDspRegs::ReadWrite);

			m_asm.eor(r, d, d, asmjit::arm::lsr(1));
			m_asm.ubfx(r, r, asmjit::Imm(54), asmjit::Imm(1));
			m_asm.bfi(sr, r, asmjit::Imm(CCRB_V), asmjit::Imm(1));
			m_asm.lsl(r, r, asmjit::Imm(CCRB_L));
			m_asm.orr(sr, sr, r.get());

			m_ccrDirty = static_cast<CCRMask>(m_ccrDirty & ~(CCR_L | CCR_V));
		}

		{
			DspValue s(m_block);
			decode_JJ_read(s, jj);

			m_asm.shl(r64(s), asmjit::Imm(40));
			m_asm.sar(r64(s), asmjit::Imm(16));

			const RegGP addOrSub(m_block);
			m_asm.eor(addOrSub, r64(s), d.get());

			m_asm.shl(d, asmjit::Imm(1));

			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), CCRB_C);
			m_asm.cinc(d, d, asmjit::arm::CondCode::kNotZero);

			const RegScratch dLsWord(m_block);
			m_asm.and_(dLsWord, d.get(), asmjit::Imm(0xffffff));

			const asmjit::Label sub = m_asm.newLabel();
			const asmjit::Label end = m_asm.newLabel();

			m_asm.bitTest(addOrSub, 55);

			m_asm.jz(sub);
			m_asm.add(d, r64(s));
			m_asm.jmp(end);

			m_asm.bind(sub);
			m_asm.sub(d, r64(s));

			m_asm.bind(end);
			m_asm.and_(d, asmjit::Imm(0xffffffffff000000));
			m_asm.orr(d, d, dLsWord);
		}

		// C is set if bit 55 of the result is cleared
		m_asm.bitTest(d, 55);
		ccr_update_ifZero(CCRB_C);

		m_dspRegs.mask56(d);
	}

	void JitOps::op_Rep_Div(const TWord _op, const TWord _iterationCount)
	{
		m_blockRuntimeData.getEncodedInstructionCount() += _iterationCount;
		m_blockRuntimeData.getEncodedCycleCount() += (_iterationCount - 1) * dsp56k::calcCycles(Div, m_pcCurrentOp + 1, _op, m_block.dsp().memory().getBridgedMemoryAddress(), 1);

		const auto ab = getFieldValue<Div, Field_d>(_op);
		const auto jj = getFieldValue<Div, Field_JJ>(_op);

		m_ccrRead |= CCR_C;
		updateDirtyCCR(CCR_C);

		AluRef d(m_block, ab);
		const auto alu = d.get();

		auto ccrUpdateVL = [&]()
		{
			// V and L updates
			// V: Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
			// L: Set if the Overflow bit (V) is set.
			// What we do is we check if bits 55 and 54 of the ALU are not identical (host parity bit cleared) and set V accordingly.
			// Nearly identical for L but L is only set, not cleared as it is sticky
			const RegGP r(m_block);
			const auto sr = m_dspRegs.getSR(JitDspRegs::ReadWrite);

			m_asm.eor(r, alu, alu, asmjit::arm::lsr(1));
			m_asm.ubfx(r, r, asmjit::Imm(54), asmjit::Imm(1));
			m_asm.bfi(sr, r, asmjit::Imm(CCRB_V), asmjit::Imm(1));
			m_asm.lsl(r, r, asmjit::Imm(CCRB_L));
			m_asm.orr(sr, sr, r.get());

			m_ccrDirty = static_cast<CCRMask>(m_ccrDirty & ~(CCR_L | CCR_V));
		};

		DspValue s(m_block, UsePooledTemp);

		decode_JJ_read(s, jj);

		RegGP addOrSub(m_block);
		RegGP carry(m_block);
		const RegGP sNeg(m_block);

		// once
		m_asm.shl(r64(s), asmjit::Imm(40));
		m_asm.sar(r64(s), asmjit::Imm(16));

		signextend56to64(alu);

		m_asm.ubfx(carry, m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_C), 1);

		const auto loopIteration = [&](bool last)
		{
			m_asm.eor(addOrSub, r64(s), alu);
			m_asm.bitTest(addOrSub, 55);
			m_asm.cneg(sNeg, r64(s), asmjit::arm::CondCode::kZero);

			m_asm.add(alu, carry.get(), alu, asmjit::arm::lsl(1));
			m_asm.add(alu, sNeg.get());

			// C is set if bit 55 of the result is cleared
			if (last)
			{
				m_asm.bitTest(alu, 55);
				ccr_update_ifZero(CCRB_C);
			}
			else
			{
				m_asm.ubfx(carry, alu, 55, 1);
				m_asm.eor(carry, carry, asmjit::Imm(1));
			}
		};

		// loop
		{
			RegGP lc(m_block);
			m_asm.mov(lc, asmjit::Imm(_iterationCount - 1));
			const auto start = m_asm.newLabel();
			m_asm.bind(start);
			loopIteration(false);
			m_asm.subs(lc, lc, asmjit::Imm(1));
			m_asm.jnz(start);
		}

		// once
		ccrUpdateVL();
		loopIteration(true);

		m_dspRegs.mask56(alu);
	}

	void JitOps::op_Not(TWord op)
	{
		const auto ab = getFieldValue<Not, Field_d>(op);

		{
			DspValue d(m_block);
			getALU1(d, ab);
			m_asm.mvn(r32(d.get()), r32(d.get()));
			m_asm.and_(d.get(), asmjit::Imm(0xffffff));
			setALU1(ab, d);

			copyBitToCCR(r32(d), 23, CCRB_N);			// Set if bit 47 of the result is set

			m_asm.test_(d.get());
			ccr_update_ifZero(CCRB_Z);					// Set if bits 47–24 of the result are 0
		}

		ccr_clear(CCR_V);								// Always cleared
	}

	void JitOps::op_Rol(TWord op)
	{
		const auto D = getFieldValue<Rol, Field_d>(op);

		DspValue r(m_block);
		getALU1(r, D);

		const RegGP prevCarry(m_block);
		ccr_getBitValue(prevCarry, CCRB_C);

		copyBitToCCR(r32(r), 23, CCRB_C);				// Set if bit 47 of the destination operand is set, and cleared otherwise

		m_asm.shl(r.get(), asmjit::Imm(1));
		ccr_n_update_by23(r64(r));						// Set if bit 47 of the result is set

		m_asm.orr(r.get(), r.get(), r32(prevCarry.get()));
		m_asm.test_(r32(r));
		ccr_update_ifZero(CCRB_Z);							// Set if bits 47–24 of the result are 0

		setALU1(D, r);

		ccr_clear(CCR_V);									// This bit is always cleared
	}

	void JitOps::op_Subl(TWord op)
	{
		const auto ab = getFieldValue<Subl, Field_d>(op);

		AluRef d(m_block, ab ? 1 : 0, true, true);

		const RegGP oldBit55(m_block);
		m_asm.bitTest(d, 55);
		m_asm.cset(r32(oldBit55), asmjit::arm::CondCode::kNotZero);

		signextend56to64(d);
		m_asm.shl(d, asmjit::Imm(1));

		{
			const RegGP s(m_block);
			signextend56to64(s, r64(m_dspRegs.getALU(ab ? 0 : 1)));

			m_asm.sub(d, s);
		}

		ccr_dirty(ab ? 1 : 0, d, static_cast<CCRMask>(CCR_E | CCR_U | CCR_N | CCR_Z));

		const RegGP newBit55(m_block);
		m_asm.bitTest(d, 55);
		m_asm.cset(r32(newBit55), asmjit::arm::CondCode::kNotZero);

		m_asm.eor(r32(oldBit55), r32(oldBit55), r32(newBit55));
		copyBitToCCR(r32(oldBit55), 0, CCRB_V);

		// Carry bit note: "The Carry bit (C) is set correctly if the source operand does not overflow as a result of the left shift operation.", we do not care at the moment
		copyBitToCCR(r32(oldBit55), 0, CCRB_C);

		m_dspRegs.mask56(d);
	}
}

#endif
