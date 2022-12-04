#include "jittypes.h"

#ifdef HAVE_ARM64

#include "jitdspmode.h"
#include "jitdspregs.h"
#include "jitops.h"

#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::ccr_clear(CCRMask _mask)
	{
		// TODO: by using BIC, we should be able to encode any kind of SR bits, this version fails on ARMv8 with "invalid immediate" if we specify more than one bit. But BIC with "Gp, Gp, Imm" is not available (yet?)
		m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(~_mask));
		ccr_clearDirty(_mask);
	}

	void JitOps::ccr_getBitValue(const JitRegGP& _dst, CCRBit _bit)
	{
		updateDirtyCCR(static_cast<CCRMask>(1 << _bit));
		sr_getBitValue(_dst, static_cast<SRBit>(_bit));
	}

	void JitOps::sr_getBitValue(const JitRegGP& _dst, SRBit _bit) const
	{
		m_asm.ubfx(_dst, m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(_bit), 1);
	}

	void JitOps::copyBitToCCR(const JitRegGP& _src, uint32_t _bitIndex, CCRBit _dstBit)
	{
		const RegGP dst(m_block);
		if(_bitIndex < 32)
			m_asm.ubfx(r32(dst), r32(_src), asmjit::Imm(_bitIndex), asmjit::Imm(1));
		else
			m_asm.ubfx(r64(dst), r64(_src), asmjit::Imm(_bitIndex), asmjit::Imm(1));

		ccr_update(dst, _dstBit);
	}

	void JitOps::ccr_update_ifZero(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kZero);
	}

	void JitOps::ccr_update_ifNotZero(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kNotZero);
	}

	void JitOps::ccr_update_ifGreater(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kGT);
	}

	void JitOps::ccr_update_ifGreaterEqual(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kGE);
	}

	void JitOps::ccr_update_ifLess(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kLT);
	}

	void JitOps::ccr_update_ifLessEqual(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kLE);
	}

	void JitOps::ccr_update_ifCarry(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kCS);
	}

	void JitOps::ccr_update_ifNotCarry(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kCC);
	}

	void JitOps::ccr_update_ifAbove(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kHI);
	}

	void JitOps::ccr_update_ifBelow(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::CondCode::kLO);
	}

	void JitOps::ccr_update(CCRBit _bit, asmjit::arm::CondCode _armConditionCode)
	{
		const RegScratch r(m_block);
		m_asm.cset(r, _armConditionCode);
		ccr_update(r, _bit);
	}

	void JitOps::ccr_update(const JitRegGP& ra, CCRBit _bit, bool _valueIsShifted/* = false*/)
	{
		assert(!_valueIsShifted);
		const auto mask = static_cast<CCRMask>(1 << _bit);

		const auto isSticky = _bit == CCRB_L || _bit == CCRB_S;

		ccr_clearDirty(mask);

		if(isSticky)
		{
			const auto sr = m_dspRegs.getSR(JitDspRegs::ReadWrite);
			m_asm.orr(sr, sr, ra, asmjit::arm::lsl(_bit));
		}
		else
			m_asm.bfi(m_dspRegs.getSR(JitDspRegs::ReadWrite), ra, asmjit::Imm(_bit), asmjit::Imm(1));
	}

	void JitOps::ccr_u_update(const JitReg64& _alu)
	{
		/*
		We want to set U if bits 47 & 46 of the ALU are identical.
		The two bits are offset by the status register scaling bits S0 and S1.
		We use the x64 parity flag for this, so we shift bits 47 & 46 down to be able to test for parity
		*/
		const auto* mode = m_block.getMode();

		if(mode)
		{
			// build shift value:
			// const auto offset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);
			const int32_t shift = 46 + (mode->testSR(SRB_S0) ? 1 : 0) - (mode->testSR(SRB_S1) ? 1 : 0);
			const RegGP r(m_block);
			m_asm.lsr(r, _alu, asmjit::Imm(shift));

			m_asm.eor(r, r, r, asmjit::arm::lsr(1));
			m_asm.ands(r, r, asmjit::Imm(0x1));
		}
		else
		{
			// build shift value:
			// const auto offset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);
			const ShiftReg shift(m_block);
			m_asm.mov(shift, asmjit::a64::regs::xzr);
			sr_getBitValue(shift, SRB_S0);
			{
				const RegGP s1(m_block);
				sr_getBitValue(s1, SRB_S1);

				m_asm.sub(shift.get(), s1.get());
			}
			const RegGP r(m_block);
			m_asm.lsr(r, _alu, asmjit::Imm(46));
			m_asm.shr(r, shift.get());	// FIXME: how can this work? shift might be negative if SRB_S1 is one but SRB_S0 is zero

			m_asm.eor(r, r, r, asmjit::arm::lsr(1));
			m_asm.ands(r, r, asmjit::Imm(0x1));
		}
		ccr_update_ifZero(CCRB_U);

		/*
		const auto sOffset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);

		const auto msb = 47 + sOffset;
		const auto lsb = 46 + sOffset;

		sr_toggle( SRB_U, bitvalue(_ab,msb) == bitvalue(_ab,lsb) );
		*/
	}

	void JitOps::ccr_e_update(const JitReg64& _alu)
	{
		/*
		Extension
		Indicates when the accumulator extension register is in use. This bit is
		cleared if all the bits of the integer portion of the 56-bit result are all
		ones or all zeros; otherwise, this bit is set. As shown below, the
		Scaling mode defines the integer portion. If the E bit is cleared, then
		the low-order fraction portion contains all the significant bits; the
		high-order integer portion is sign extension. In this case, the
		accumulator extension register can be ignored.
		S1 / S0 / Scaling Mode / Integer Portion
		0	0	No Scaling	Bits 55,54..............48,47
		0	1	Scale Down	Bits 55,54..............49,48
		1	0	Scale Up	Bits 55,54..............47,46
		*/

		{
			const RegGP mask(m_block);

			const auto* mode = m_block.getMode();

			if(mode)
			{
				uint32_t m = 0x3fe;
				if(mode->testSR(SRB_S0))	m <<= 1;
				if(mode->testSR(SRB_S1))	m >>= 1;
				m &= 0x3ff;
				m_asm.mov(mask, asmjit::Imm(m));
			}
			else
			{
				m_asm.mov(mask, asmjit::Imm(0x3fe));

				{
					const ShiftReg s0s1(m_block);
					m_asm.mov(s0s1, asmjit::a64::regs::xzr);

					sr_getBitValue(s0s1, SRB_S0);
					m_asm.shl(mask, s0s1.get());
					sr_getBitValue(s0s1, SRB_S1);
					m_asm.shr(mask, s0s1.get());
				}

				m_asm.and_(mask, asmjit::Imm(0x3ff));
			}
			{
				const RegScratch alu(m_block);
				m_asm.mov(alu, _alu);
				m_asm.shr(alu, asmjit::Imm(46));
				m_asm.and_(alu, mask.get());

				// res = alu != mask && alu != 0

				// Don't be distracted by the names, we abuse alu & mask here to store comparison results
				m_asm.cmp(alu, mask.get());			m_asm.cset(mask, asmjit::arm::CondCode::kNotZero);
				m_asm.test_(alu);					m_asm.cset(alu, asmjit::arm::CondCode::kNotZero);

				m_asm.and_(mask.get(), alu);
			}

			ccr_update(mask, CCRB_E);
		}
	}

	void JitOps::ccr_n_update_by55(const JitReg64& _alu)
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bitTest(_alu, 55);
		ccr_update_ifNotZero(CCRB_N);
	}

	void JitOps::ccr_n_update_by47(const JitReg64& _alu)
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bitTest(_alu, 47);
		ccr_update_ifNotZero(CCRB_N);
	}

	void JitOps::ccr_n_update_by23(const JitReg64& _alu)
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bitTest(_alu, 23);
		ccr_update_ifNotZero(CCRB_N);
	}

	void JitOps::ccr_s_update(const JitReg64& _alu)
	{
		const auto exit = m_asm.newLabel();

		m_asm.tbnz(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_S), exit);

		const auto* mode = m_block.getMode();

		if(mode)
		{
			const auto bit = 45 + (mode->testSR(SRB_S1) ? 1 : 0) - (mode->testSR(SRB_S0) ? 1 : 0);

			const RegGP alu(m_block);
			m_asm.lsr(alu, _alu, asmjit::Imm(bit));
			m_asm.eor(alu, alu, alu, asmjit::arm::lsr(1));
			m_asm.ands(alu, alu, asmjit::Imm(1));
		}
		else
		{
			const RegGP bit(m_block);
			m_asm.mov(bit, asmjit::Imm(45));

			{
				const RegGP s0s1(m_block);
				m_asm.mov(s0s1, asmjit::a64::regs::xzr);
				sr_getBitValue(s0s1, SRB_S1);
				m_asm.add(bit, s0s1.get());

				sr_getBitValue(s0s1, SRB_S0);
				m_asm.sub(bit, s0s1.get());
			}

			const RegGP alu(m_block);
			m_asm.lsr(alu, _alu, bit.get());
			m_asm.eor(alu, alu, alu, asmjit::arm::lsr(1));
			m_asm.ands(alu, alu, asmjit::Imm(1));
		}

		ccr_update_ifNotZero(CCRB_S);

		m_asm.bind(exit);
	}

	void JitOps::ccr_l_update_by_v()
	{
		updateDirtyCCR(CCR_V);

		const auto sr = m_block.regs().getSR(JitDspRegs::ReadWrite);

		{
			const RegScratch r(m_block);
			m_asm.ubfx(r, sr, asmjit::Imm(CCRB_V), asmjit::Imm(1));
			m_asm.shl(r, asmjit::Imm(CCRB_L));

			m_asm.or_(sr, r);
		}

		ccr_clearDirty(CCR_L);
	}
}

#endif
