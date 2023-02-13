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
		m_ccrWritten |= _mask;

		// TODO: by using BIC, we should be able to encode any kind of SR bits, this version fails on ARMv8 with "invalid immediate" if we specify more than one bit. But BIC with "Gp, Gp, Imm" is not available (yet?)
		m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(~_mask));
		ccr_clearDirty(_mask);
	}

	void JitOps::ccr_getBitValue(const JitRegGP& _dst, CCRBit _bit)
	{
		const auto mask = static_cast<CCRMask>(1 << _bit);
		m_ccrRead |= mask;
		updateDirtyCCRWithTemp(_dst, mask);
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
		m_ccrWritten |= mask;

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
		*/
		const auto* mode = m_block.getMode();

		if(mode)
		{
			// build shift value:
			// const auto offset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);
			const int32_t shift = 46 + (mode->testSR(SRB_S0) ? 1 : 0) - (mode->testSR(SRB_S1) ? 1 : 0);
			const RegGP r(m_block);
			m_asm.lsr(r, _alu, asmjit::Imm(shift));

			m_asm.eon(r, r, r, asmjit::arm::lsr(1));
			copyBitToCCR(r, 0, CCRB_U);
		}
		else
		{
			// build shift value:
			// const auto offset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);
			const ShiftReg shift(m_block);
			sr_getBitValue(shift, SRB_S0);
			{
				const RegGP s1(m_block);
				sr_getBitValue(s1, SRB_S1);

				m_asm.sub(shift.get(), s1.get());
			}
			const RegGP r(m_block);
			m_asm.lsr(r, _alu, asmjit::Imm(46));
			m_asm.shr(r, shift.get());	// FIXME: how can this work? shift might be negative if SRB_S1 is one but SRB_S0 is zero

			m_asm.eon(r, r, r, asmjit::arm::lsr(1));
			copyBitToCCR(r, 0, CCRB_U);
		}

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
		Family Manual P 5-15
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

		The emu approach:
		We sign-extend the alu first and then right-shift all bits in question. This will result
		In a temp reg that is either all ones or all zeroes if the extension is clear
		We increment by 1 to have either 0 or 1 and do an unsigned compare if the result is <= 1
		*/

		auto aluScratch = [this, &_alu]()
		{
			RegScratch alu(m_block);
			m_asm.lsl(alu, _alu, asmjit::Imm(8));
			return alu;
		};

		auto compare = [this](const RegScratch& _aluScratch)
		{
			m_asm.inc(r32(_aluScratch));
			m_asm.cmp(r32(_aluScratch), asmjit::Imm(1));
		};

		const auto* mode = m_block.getMode();

		if(mode)
		{
			uint32_t shift = 47;
			if(mode->testSR(SRB_S0))	++shift;
			if(mode->testSR(SRB_S1))	--shift;

			const auto alu = aluScratch();

			m_asm.sar(alu, asmjit::Imm(shift + 8));

			compare(alu);
		}
		else
		{
			const ShiftReg shift(m_block);
			{
				const RegScratch bitVal(m_block);
				sr_getBitValue(bitVal, SRB_S0);
				m_asm.add(r32(shift), r32(bitVal), asmjit::Imm(47 + 8));
				sr_getBitValue(bitVal, SRB_S1);
				m_asm.sub(r32(shift), r32(bitVal));
			}

			const auto alu = aluScratch();

			m_asm.asr(r64(alu), r64(alu), r64(shift.get()));

			compare(alu);
		}

		ccr_update(CCRB_E, asmjit::arm::CondCode::kUnsignedGT);
	}

	void JitOps::ccr_n_update_by55(const JitReg64& _alu)
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		copyBitToCCR(_alu, 55, CCRB_N);
	}

	void JitOps::ccr_n_update_by47(const JitReg64& _alu)
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		copyBitToCCR(_alu, 47, CCRB_N);
	}

	void JitOps::ccr_n_update_by23(const JitReg64& _alu)
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		copyBitToCCR(_alu, 23, CCRB_N);
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
			copyBitToCCR(alu, 0, CCRB_S);
		}
		else
		{
			const RegGP bit(m_block);

			{
				const RegGP s0s1(m_block);
				sr_getBitValue(s0s1, SRB_S1);
				m_asm.add(bit, s0s1.get(), asmjit::Imm(45));

				sr_getBitValue(s0s1, SRB_S0);
				m_asm.sub(bit, s0s1.get());
			}

			const RegGP alu(m_block);
			m_asm.lsr(alu, _alu, bit.get());
			m_asm.eor(alu, alu, alu, asmjit::arm::lsr(1));
			copyBitToCCR(alu, 0, CCRB_S);
		}

		m_asm.bind(exit);
	}

	void JitOps::ccr_l_update_by_v()
	{
		assert((m_ccrDirty & CCR_V) == 0);
		updateDirtyCCR(CCR_V);
		m_ccrWritten |= CCR_L;

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
