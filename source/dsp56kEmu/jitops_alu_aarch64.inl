#pragma once

#include "jitops.h"
#include "jittypes.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::XY0to56(const JitReg64& _dst, int _xy) const
	{
		const auto src = m_dspRegs.getXY(_xy, JitDspRegs::Read);

		// We might be able to save one op by using SBFM but unfortunately the docs are really bad
		m_asm.lsl(_dst, src, asmjit::Imm(40));
		m_asm.asr(_dst, _dst, asmjit::Imm(8));
		m_asm.lsr(_dst, _dst, asmjit::Imm(8));
	}

	void JitOps::XY1to56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shr(_dst, asmjit::Imm(24));	// remove LSWord
		signed24To56(_dst);
	}

	inline void JitOps::alu_abs(const JitRegGP& _r)
	{
		m_asm.cmp(_r, asmjit::Imm(0));
		m_asm.cneg(_r, _r, asmjit::arm::Cond::kLT);			// negate if < 0
	}
	
	inline void JitOps::alu_and(const TWord ab, RegGP& _v)
	{
		m_asm.shl(_v, asmjit::Imm(24));

		AluRef alu(m_block, ab);

		{
			const RegGP r(m_block);
			m_asm.ands(r, alu.get(), _v.get());
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

	inline void JitOps::alu_asl(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluReg alu(m_block, _abDst, false, _abDst != _abSrc);
		if (_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.bitTest(alu, 55);
		ccr_update_ifNotZero(CCRB_C);

		m_asm.lsl(alu, alu, asmjit::Imm(8));				// we want to hit the 64 bit boundary to be able to check for overflow
		m_asm.lsl(alu, alu, _v.get());

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical ot the backshifted one
		{
			AluReg oldAlu(m_block, _abSrc, true);
			m_asm.lsl(oldAlu, oldAlu, asmjit::Imm(8));
			m_asm.asr(alu, alu, _v.get());
			m_asm.cmp(alu, oldAlu.get());
		}

		ccr_update_ifNotZero(CCRB_V);

		m_asm.lsl(alu, alu, _v.get());					// one more time
		m_asm.lsr(alu, alu, asmjit::Imm(8));			// correction

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}
	
	inline void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.lsl(alu, alu, asmjit::Imm(8));	// make sign-extend possible in our wide registers
		m_asm.asr(alu, alu, _v.get());
		m_asm.bitTest(alu, 7);					// carry is the last bit shifted out, we can grab it at bit pos 7 now as we pre-shifted left by 8
		ccr_update_ifNotZero(CCRB_C);
		m_asm.lsr(alu, alu, asmjit::Imm(8));	// correction

		ccr_clear(CCR_V);
		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_bclr(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.bitTest(_dst, _bit);
		ccr_update_ifNotZero(CCRB_C);
		m_asm.and_(_dst, _dst, asmjit::Imm(~(1ull << _bit)));
	}

	inline void JitOps::alu_bset(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.bitTest(_dst, _bit);
		ccr_update_ifNotZero(CCRB_C);
		m_asm.orr(_dst, _dst, asmjit::Imm(1ull << _bit));
	}

	inline void JitOps::alu_bchg(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.bitTest(_dst, _bit);
		ccr_update_ifNotZero(CCRB_C);
		m_asm.eor(_dst, _dst, asmjit::Imm(1ull << _bit));
	}

	inline void JitOps::alu_lsl(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.bitTest(d, 24 - _shiftAmount);
		ccr_update_ifNotZero(CCRB_C);
		m_asm.shl(r32(d.get()), _shiftAmount + 8);	// + 8 to be able to check against zero because we move the MSB out or the register
		m_asm.shr(r32(d.get()), 8);					// revert shift by 8
		m_asm.cmp(r32(d.get()), asmjit::Imm(0));
		ccr_update_ifZero(CCRB_Z);
		m_asm.bitTest(r32(d.get()), 23);
		ccr_update_ifNotZero(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, r32(d.get()));
	}

	inline void JitOps::alu_lsr(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.bitTest(d, _shiftAmount - 1);
		ccr_update_ifNotZero(CCRB_C);
		m_asm.shr(r32(d.get()), _shiftAmount);
		m_asm.cmp(r32(d.get()), asmjit::Imm(0));
		ccr_update_ifZero(CCRB_Z);
		m_asm.bitTest(r32(d.get()), 23);
		ccr_update_ifNotZero(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, r32(d.get()));
	}

	inline void JitOps::alu_rnd(TWord ab, const JitReg64& d)
	{
		RegGP rounder(m_block);
		m_asm.mov(rounder, asmjit::Imm(0x800000));

		{
			const ShiftReg shifter(m_block);
			m_asm.mov(shifter, asmjit::a64::xzr);
			sr_getBitValue(shifter, SRB_S1);
			m_asm.shr(rounder, shifter.get());
			sr_getBitValue(shifter, SRB_S0);
			m_asm.shl(rounder, shifter.get());
		}

		signextend56to64(d);
		m_asm.add(d, rounder.get());

		m_asm.shl(rounder, asmjit::Imm(1));

		{
			// mask = all the bits to the right of, and including the rounding position
			const RegGP mask(m_block);
			m_asm.sub(mask, rounder, asmjit::Imm(1));

			const auto skipNoScalingMode = m_asm.newLabel();

			// if (!sr_test_noCache(SR_RM))
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_SM);
			m_asm.cond_not_zero().b(skipNoScalingMode);

			// convergent rounding. If all mask bits are cleared

			// then the bit to the left of the rounding position is cleared in the result
			// if (!(_alu.var & mask)) 
			//	_alu.var&=~(rounder<<1);
			m_asm.mvn(rounder, rounder);

			{
				const RegGP aluIfAndWithMaskIsZero(m_block);
				m_asm.mov(aluIfAndWithMaskIsZero, d);
				m_asm.and_(aluIfAndWithMaskIsZero, rounder.get());

				rounder.release();

				m_asm.tst(d, mask.get());
				m_asm.csel(d, aluIfAndWithMaskIsZero.get(), d, asmjit::arm::Cond::kZero);
			}

			m_asm.bind(skipNoScalingMode);

			// all bits to the right of and including the rounding position are cleared.
			// _alu.var&=~mask;
			m_asm.mvn(mask, mask);
			m_asm.and_(d, mask.get());
		}

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z | CCR_V));
		m_dspRegs.mask56(d);
	}

	inline void JitOps::op_Btst_ea(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_ea>(r, op);
		m_asm.bitTest(r32(r.get()), getBit<Btst_ea>(op));
		ccr_update_ifNotZero(CCRB_C);
	}

	inline void JitOps::op_Btst_aa(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_aa>(r, op);
		m_asm.bitTest(r32(r.get()), getBit<Btst_aa>(op));
		ccr_update_ifNotZero(CCRB_C);
	}

	inline void JitOps::op_Btst_pp(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_pp>(r, op);
		m_asm.bitTest(r32(r.get()), getBit<Btst_pp>(op));
		ccr_update_ifNotZero(CCRB_C);
	}

	inline void JitOps::op_Btst_qq(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_qq>(r, op);
		m_asm.bitTest(r32(r.get()), getBit<Btst_qq>(op));
		ccr_update_ifNotZero(CCRB_C);
	}

	inline void JitOps::op_Btst_D(TWord op)
	{
		const auto dddddd = getFieldValue<Btst_D, Field_DDDDDD>(op);
		const auto bit = getBit<Btst_D>(op);

		const RegGP r(m_block);
		decode_dddddd_read(r32(r.get()), dddddd);

		m_asm.bitTest(r32(r.get()), bit);
		ccr_update_ifNotZero(CCRB_C);
	}

	inline void JitOps::op_Div(TWord op)
	{
		const auto ab = getFieldValue<Div, Field_d>(op);
		const auto jj = getFieldValue<Div, Field_JJ>(op);

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
			const RegGP s(m_block);
			decode_JJ_read(s, jj);

			m_asm.shl(s, asmjit::Imm(40));
			m_asm.sar(s, asmjit::Imm(16));

			const RegGP addOrSub(m_block);
			m_asm.orr(addOrSub, s.get(), d.get());

			m_asm.shl(d, asmjit::Imm(1));

			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), CCRB_C);
			m_asm.cinc(d, d, asmjit::arm::Cond::kNotZero);

			const auto dLsWord = regReturnVal;
			m_asm.and_(dLsWord, d.get(), asmjit::Imm(0xffffff));

			const asmjit::Label sub = m_asm.newLabel();
			const asmjit::Label end = m_asm.newLabel();

			m_asm.bitTest(addOrSub, 55);

			m_asm.cond_zero().b(sub);
			m_asm.add(d, s.get());
			m_asm.jmp(end);

			m_asm.bind(sub);
			m_asm.sub(d, s.get());

			m_asm.bind(end);
			m_asm.and_(d, asmjit::Imm(0xffffffffff000000));
			m_asm.orr(d, d, dLsWord);
		}

		// C is set if bit 55 of the result is cleared
		m_asm.bitTest(d, 55);
		ccr_update_ifZero(CCRB_C);

		m_dspRegs.mask56(d);
	}

	inline void JitOps::op_Rep_Div(const TWord _op, const TWord _iterationCount)
	{
		m_block.getEncodedInstructionCount() += _iterationCount;

		const auto ab = getFieldValue<Div, Field_d>(_op);
		const auto jj = getFieldValue<Div, Field_JJ>(_op);

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

		const auto finished = m_asm.newLabel();
		const auto regular = m_asm.newLabel();
		const RegGP s(m_block);
		decode_JJ_read(s, jj);
		/* TODO: broken
		if (_iterationCount<24)
		{
			{
				const RegGP t(m_block);
				m_asm.mov(t, s.get());
				m_asm.dec(t);
				m_asm.and_(t, s.get());
				m_asm.jnz(regular);
			}
			{
				const ShiftReg t(m_block);
				m_asm.bsr(t, s.get());
				m_asm.add(t, asmjit::Imm(_iterationCount + 1));
				m_asm.shr(d, t.get());
				m_asm.and_(d, asmjit::Imm((1<<_iterationCount)-1));
				m_asm.jmp(finished);
			}
		}
		*/
		m_asm.bind(regular);
		RegGP addOrSub(m_block);
		RegGP lc(m_block);
		RegGP carry(m_block);

		const auto loopIteration = [&](bool last)
		{
			m_asm.orr(addOrSub, s.get(), alu);

			m_asm.shl(alu, asmjit::Imm(1));

			m_asm.add(alu, alu, carry.get());

			const auto dLsWord = regReturnVal;
			m_asm.and_(dLsWord, alu, asmjit::Imm(0xffffff));

			const asmjit::Label sub = m_asm.newLabel();
			const asmjit::Label end = m_asm.newLabel();

			m_asm.bitTest(addOrSub, 55);

			m_asm.cond_zero().b(sub);
			m_asm.add(alu, s.get());
			m_asm.jmp(end);

			m_asm.bind(sub);
			m_asm.sub(alu, s.get());

			m_asm.bind(end);
			m_asm.and_(alu, asmjit::Imm(0xffffffffff000000));
			m_asm.orr(alu, alu, dLsWord);

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

		// once
		m_asm.shl(s, asmjit::Imm(40));
		m_asm.sar(s, asmjit::Imm(16));

		m_asm.mov(lc, _iterationCount - 1);

		m_asm.ubfx(carry, m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_C), 1);

		// loop
		const auto start = m_asm.newLabel();
		m_asm.bind(start);
		loopIteration(false);
		m_asm.subs(lc, lc, asmjit::Imm(1));
		m_asm.cond_not_zero().b(start);
		lc.release();

		// once
		ccrUpdateVL();
		loopIteration(true);

		m_dspRegs.mask56(alu);
		m_asm.bind(finished);
	}

	inline void JitOps::op_Not(TWord op)
	{
		const auto ab = getFieldValue<Not, Field_d>(op);

		{
			const RegGP d(m_block);
			getALU1(d, ab);
			m_asm.mvn(r32(d.get()), r32(d.get()));
			m_asm.and_(d, asmjit::Imm(0xffffff));
			setALU1(ab, r32(d.get()));

			m_asm.bitTest(d, 23);
			ccr_update_ifNotZero(CCRB_N);				// Set if bit 47 of the result is set

			m_asm.cmp(d, asmjit::Imm(0));
			ccr_update_ifZero(CCRB_Z);					// Set if bits 47–24 of the result are 0
		}

		ccr_clear(CCR_V);								// Always cleared
	}

	inline void JitOps::op_Rol(TWord op)
	{
		const auto D = getFieldValue<Rol, Field_d>(op);

		RegGP r(m_block);
		getALU1(r, D);

		const RegGP prevCarry(m_block);
		ccr_getBitValue(prevCarry, CCRB_C);

		m_asm.bitTest(r, 23);								// Set if bit 47 of the destination operand is set, and cleared otherwise
		ccr_update_ifNotZero(CCRB_C);

		m_asm.shl(r.get(), asmjit::Imm(1));
		ccr_n_update_by23(r.get());							// Set if bit 47 of the result is set

		m_asm.orr(r, r, prevCarry.get());					// Set if bits 47–24 of the result are 0
		ccr_update_ifZero(CCRB_Z);
		setALU1(D, r32(r.get()));

		ccr_clear(CCR_V);									// This bit is always cleared
	}
}
