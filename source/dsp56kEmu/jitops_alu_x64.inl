#pragma once

#include "jitops.h"
#include "jittypes.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::XY0to56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shl(_dst, asmjit::Imm(40));
		m_asm.sar(_dst, asmjit::Imm(8));
		m_asm.shr(_dst, asmjit::Imm(8));
	}

	void JitOps::XY1to56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shr(_dst, asmjit::Imm(24));	// remove LSWord
		signed24To56(_dst);
	}

	inline void JitOps::alu_abs(const JitRegGP& _r)
	{
		const auto rb = regReturnVal;

		m_asm.mov(rb, _r);		// Copy to backup location
		m_asm.neg(_r);			// negate
		m_asm.cmovl(_r, rb);	// if now negative, restore its saved value
	}

	inline void JitOps::alu_and(const TWord ab, RegGP& _v)
	{
		m_asm.shl(_v, asmjit::Imm(24));

		AluRef alu(m_block, ab);

		{
			const RegGP r(m_block);
			m_asm.mov(r, alu.get());
			m_asm.and_(r, _v.get());
			ccr_update_ifZero(CCRB_Z);
		}

		{
			const auto mask = regReturnVal;
			m_asm.mov(mask, asmjit::Imm(0xff000000ffffff));
			m_asm.or_(_v, mask);
			m_asm.and_(alu, _v.get());
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

		m_asm.sal(alu, asmjit::Imm(8));				// we want to hit the 64 bit boundary to make use of the native carry flag so pre-shift by 8 bit (56 => 64)

		m_asm.sal(alu, _v.get());					// now do the real shift

		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical ot the backshifted one
		{
			AluReg oldAlu(m_block, _abSrc, true);
			m_asm.sal(oldAlu, asmjit::Imm(8));
			m_asm.sar(alu, _v.get());
			m_asm.cmp(alu, oldAlu.get());
		}

		ccr_update_ifNotZero(CCRB_V);

		m_asm.sal(alu, _v.get());					// one more time
		m_asm.shr(alu, asmjit::Imm(8));				// correction

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.sal(alu, asmjit::Imm(8));
		m_asm.sar(alu, _v.get());
		m_asm.sar(alu, asmjit::Imm(8));
		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag
		m_dspRegs.mask56(alu);

		ccr_clear(CCR_V);

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_bclr(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.btr(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::alu_bset(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.bts(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::alu_bchg(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.btc(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::alu_lsl(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.shl(r32(d.get()), _shiftAmount + 8);	// + 8 to use native carry flag
		ccr_update_ifCarry(CCRB_C);
		m_asm.shr(r32(d.get()), 8);				// revert shift by 8
		ccr_update_ifZero(CCRB_Z);
		m_asm.bt(r32(d.get()), asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, r32(d.get()));
	}

	inline void JitOps::alu_lsr(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.shr(r32(d.get()), _shiftAmount);
		ccr_update_ifCarry(CCRB_C);
		m_asm.cmp(r32(d.get()), asmjit::Imm(0));
		ccr_update_ifZero(CCRB_Z);
		m_asm.bt(r32(d.get()), asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, r32(d.get()));
	}

	inline void JitOps::alu_rnd(TWord ab, const JitReg64& d)
	{
		RegGP rounder(m_block);
		m_asm.mov(rounder, asmjit::Imm(0x800000));

		{
			const ShiftReg shifter(m_block);
			m_asm.xor_(shifter, shifter.get());
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
			m_asm.mov(mask, rounder.get());
			m_asm.dec(mask);

			const auto skipNoScalingMode = m_asm.newLabel();

			// if (!sr_test_noCache(SR_RM))
			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_SM));
			m_asm.jc(skipNoScalingMode);

			// convergent rounding. If all mask bits are cleared

			// then the bit to the left of the rounding position is cleared in the result
			// if (!(_alu.var & mask)) 
			//	_alu.var&=~(rounder<<1);
			m_asm.not_(rounder);

			{
				const RegGP aluIfAndWithMaskIsZero(m_block);
				m_asm.mov(aluIfAndWithMaskIsZero, d);
				m_asm.and_(aluIfAndWithMaskIsZero, rounder.get());

				rounder.release();

				{
					const auto temp = regReturnVal;
					m_asm.mov(temp, d);
					m_asm.and_(temp, mask.get());
					m_asm.cmovz(d, aluIfAndWithMaskIsZero.get());
				}
			}

			m_asm.bind(skipNoScalingMode);

			// all bits to the right of and including the rounding position are cleared.
			// _alu.var&=~mask;
			m_asm.not_(mask);
			m_asm.and_(d, mask.get());
		}

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z | CCR_V));
		m_dspRegs.mask56(d);
	}
}
