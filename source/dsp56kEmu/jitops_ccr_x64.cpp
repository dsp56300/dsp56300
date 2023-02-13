#include "jitdspmode.h"
#include "jittypes.h"

#ifdef HAVE_X86_64

#include "jitdspregs.h"
#include "jitops.h"

#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::ccr_clear(CCRMask _mask)
	{
		m_ccrWritten |= _mask;
		m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite).r32(), asmjit::Imm(~static_cast<uint32_t>(_mask)));
		ccr_clearDirty(_mask);
	}

	void JitOps::ccr_getBitValue(const JitRegGP& _dst, CCRBit _bit)
	{
		const auto mask = static_cast<CCRMask>(1 << _bit);
		m_ccrRead |= mask;
		updateDirtyCCR(mask);
		m_asm.copyBitToReg(_dst, m_dspRegs.getSR(JitDspRegs::Read), _bit);
	}

	void JitOps::sr_getBitValue(const JitRegGP& _dst, SRBit _bit) const
	{
		m_asm.copyBitToReg(_dst, m_dspRegs.getSR(JitDspRegs::Read), _bit);
	}

	void JitOps::copyBitToCCR(const JitRegGP& _src, uint32_t _bitIndex, CCRBit _dstBit)
	{
		const RegScratch dst(m_block);
		m_asm.copyBitToReg(dst, _dstBit, _src, _bitIndex);
		ccr_update(dst, _dstBit, true);
	}

	void JitOps::ccr_update_ifZero(CCRBit _bit)
	{
		// set reg to 1 if last operation returned zero, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kZero);
	}

	void JitOps::ccr_update_ifNotZero(CCRBit _bit)
	{
		// set reg to 1 if last operation returned != 0, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kNotZero);
	}

	void JitOps::ccr_update_ifGreater(CCRBit _bit)
	{
		// set reg to 1 if last operation returned >, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kG);
	}

	void JitOps::ccr_update_ifGreaterEqual(CCRBit _bit)
	{
		// set reg to 1 if last operation returned >=, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kGE);
	}

	void JitOps::ccr_update_ifLess(CCRBit _bit)
	{
		// set reg to 1 if last operation returned <, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kL);
	}

	void JitOps::ccr_update_ifLessEqual(CCRBit _bit)
	{
		// set reg to 1 if last operation returned <=, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kLE);
	}

	void JitOps::ccr_update_ifCarry(CCRBit _bit)
	{
		// set reg to 1 if last operation generated carry, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kC);
	}

	void JitOps::ccr_update_ifNotCarry(CCRBit _bit)
	{
		// set reg to 1 if last operation did NOT generate carry, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kNC);
	}

	void JitOps::ccr_update_ifParity(CCRBit _bit)
	{
		// set reg to 1 if number of 1 bits is even, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kP);
	}

	void JitOps::ccr_update_ifNotParity(CCRBit _bit)
	{
		// set reg to 1 if number of 1 bits is odd, 0 otherwise
		ccr_update(_bit, asmjit::x86::CondCode::kNP);
	}

	void JitOps::ccr_update_ifAbove(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::x86::CondCode::kA);
	}

	void JitOps::ccr_update_ifBelow(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::x86::CondCode::kB);
	}

	void JitOps::ccr_update(CCRBit _bit, asmjit::x86::CondCode _cc)
	{
		const RegScratch r(m_block);
		m_asm.set(_cc, r.r8());
		ccr_update(r, _bit);
	}

	void JitOps::ccr_update(const JitRegGP& ra, CCRBit _bit, bool _valueIsShifted/* = false*/)
	{
		const auto mask = static_cast<CCRMask>(1 << _bit);
		m_ccrWritten |= mask;

		if (m_ccr_update_clear && _bit != CCRB_L && _bit != CCRB_S)
			ccr_clear(mask);												// clear out old status register value
		else
			ccr_clearDirty(mask);

		if (_bit && !_valueIsShifted)
			m_asm.shl(ra.r8(), _bit);										// shift left to become our new SR bit
		m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite).r8(), ra.r8());	// or in our new SR bit
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
			const uint32_t shift = 46 + (mode->testSR(SRB_S0) ? 1 : 0) - (mode->testSR(SRB_S1) ? 1 : 0);
			const RegScratch r(m_block);
			m_asm.ror(r, _alu, static_cast<int>(shift));
			m_asm.test(r32(r), asmjit::Imm(0x3));
		}
		else
		{
			// build shift value:
			// const auto offset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);
			const ShiftReg shift64(m_block);
			const auto shift = shift64.get().r8();
			sr_getBitValue(shift, SRB_S0);
			m_asm.add(shift, asmjit::Imm(46));
			{
				const RegGP s1(m_block);
				sr_getBitValue(s1, SRB_S1);

				m_asm.sub(shift, s1.get().r8());
			}
			const RegScratch r(m_block);
			m_asm.mov(r, _alu);
			m_asm.shr(r, shift);
			m_asm.test(r32(r), asmjit::Imm(0x3));
		}
		ccr_update_ifParity(CCRB_U);

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
			m_asm.rol(alu, _alu, 8);
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
			m_asm.mov(r32(shift), asmjit::Imm(47 + 8));
			{
				const RegScratch bitVal(m_block);
				sr_getBitValue(bitVal, SRB_S0);
				m_asm.add(shift.get().r8(), bitVal.get().r8());
				sr_getBitValue(bitVal, SRB_S1);
				m_asm.sub(shift.get().r8(), bitVal.get().r8());
			}

			const auto alu = aluScratch();

			m_asm.sar(alu, shift.get().r8());

			compare(alu);
		}

		ccr_update(CCRB_E, asmjit::x86::CondCode::kUnsignedGT);
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

		m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), CCRB_S);
		m_asm.jnz(exit);

		const auto* mode = m_block.getMode();

		if(mode)
		{
			uint32_t bit = 46 + (mode->testSR(SRB_S1) ? 1 : 0) - (mode->testSR(SRB_S0) ? 1 : 0);

			const RegGP bit46(m_block);
			m_asm.copyBitToReg(bit46, _alu, bit);

			const RegGP temp(m_block);
			m_asm.copyBitToReg(temp, _alu, bit - 1);

			m_asm.xor_(temp, bit46.get());

			ccr_update(temp, CCRB_S);
		}
		else
		{
			const RegGP bit(m_block);
			m_asm.mov(bit, asmjit::Imm(46));

			{
				const RegGP s0s1(m_block);
				m_asm.xor_(s0s1, s0s1.get());
				sr_getBitValue(s0s1, SRB_S1);
				m_asm.add(bit, s0s1.get());

				sr_getBitValue(s0s1, SRB_S0);
				m_asm.sub(bit, s0s1.get());
			}

			const RegGP bit46(m_block);
			m_asm.bt(_alu, bit.get());
			m_asm.setc(bit46.get().r8());

			m_asm.dec(bit);
			m_asm.bt(_alu, bit.get());
			m_asm.setc(bit.get().r8());
			m_asm.xor_(bit, bit46.get());

			ccr_update(bit, CCRB_S);
		}

		m_asm.bind(exit);
	}

	void JitOps::ccr_l_update_by_v()
	{
		assert((m_ccrDirty & CCR_V) == 0);
		updateDirtyCCR(CCR_V);
		m_ccrWritten |= CCR_L;

		static_assert(CCRB_L > CCRB_V);
		constexpr auto shift = CCRB_L - CCRB_V;

		const auto sr = r32(m_block.regs().getSR(JitDspRegs::ReadWrite));

		const RegScratch temp(m_block);
		m_asm.rol(r32(temp), sr, shift);
		m_asm.and_(r32(temp), asmjit::Imm(CCR_L));
		m_asm.or_(sr, r32(temp));

		ccr_clearDirty(CCR_L);
	}
}

#endif
