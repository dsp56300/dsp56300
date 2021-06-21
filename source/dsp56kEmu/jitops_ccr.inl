#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "jitregtypes.h"

namespace dsp56k
{
	inline void JitOps::ccr_clear(CCRMask _mask) const
	{
		m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(~_mask));
	}

	inline void JitOps::ccr_set(CCRMask _mask) const
	{
		m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(_mask));
	}

	inline void JitOps::ccr_dirty(const JitReg64& _alu)
	{
		if(m_useCCRCache)
		{
			m_asm.movq(regLastModAlu, _alu);
			m_ccrDirty = true;
		}
		else
		{
			updateDirtyCCR(_alu);
		}
	}

	inline void JitOps::updateDirtyCCR()
	{
		if(!m_ccrDirty)
			return;

		const RegGP r(m_block);
		m_asm.movq(r, regLastModAlu);
		updateDirtyCCR(r);
	}

	inline void JitOps::updateDirtyCCR(const JitReg64& _alu)
	{
		ccr_e_update(_alu);
		ccr_u_update(_alu);
		m_ccrDirty = false;
	}

	inline void JitOps::sr_getBitValue(const JitReg& _dst, CCRBit _bit) const
	{
		m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(_bit));
		m_asm.setc(_dst);
	}

	inline void JitOps::ccr_update_ifZero(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setz(ra);										// set reg to 1 if last operation returned zero, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotZero(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setnz(ra);									// set reg to 1 if last operation returned != 0, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreater(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setg(ra);										// set reg to 1 if last operation returned >, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreaterEqual(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setge(ra);									// set reg to 1 if last operation returned >=, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifLess(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setl(ra);										// set reg to 1 if last operation returned <, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifLessEqual(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setle(ra);									// set reg to 1 if last operation returned <=, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifCarry(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setc(ra);										// set reg to 1 if last operation generated carry, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotCarry(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setnc(ra);									// set reg to 1 if last operation did NOT generate carry, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifParity(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setp(ra);										// set reg to 1 if number of 1 bits is even, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotParity(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setnp(ra);									// set reg to 1 if number of 1 bits is odd, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifAbove(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.seta(ra);
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifBelow(CCRBit _bit) const
	{
		const RegGP ra(m_block);
		m_asm.setb(ra);
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update(const JitReg& ra, CCRBit _bit) const
	{
		if(_bit != CCRB_L && _bit != CCRB_S)
			ccr_clear(static_cast<CCRMask>(1 << _bit));						// clear out old status register value

		if(_bit)
			m_asm.shl(ra.r8(), _bit);										// shift left to become our new SR bit
		m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite).r8(), ra.r8());	// or in our new SR bit
	}

	void JitOps::ccr_u_update(const JitReg64& _alu) const
	{
		/*
		We want to set U if bits 47 & 46 of the ALU are identical.
		The two bits are offset by the status register scaling bits S0 and S1.
		We use the x64 parity flag for this, so we shift bits 47 & 46 down to be able to test for parity
		*/
		{
			// build shift value:
			// const auto offset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);
			const PushGP shift(m_block, asmjit::x86::rcx);
			m_asm.xor_(shift,shift.get());
			sr_getBitValue(shift, SRB_S0);
			m_asm.add(shift, asmjit::Imm(14));	// 46 - 32
			{
				const RegGP s1(m_block);
				sr_getBitValue(s1, SRB_S1);			

				m_asm.sub(shift.get().r8(), s1.get().r8());
			}
			const RegGP r(m_block);
			m_asm.mov(r,_alu);
			m_asm.shr(r, asmjit::Imm(32));	// thx to intel, we are only allowed to shift 32 at max. Therefore, we need to split it
			m_asm.shr(r, shift.get().r8());
			m_asm.and_(r, asmjit::Imm(0x3));
		}
		ccr_update_ifParity(CCRB_U);

		/*
		const auto sOffset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);

		const auto msb = 47 + sOffset;
		const auto lsb = 46 + sOffset;

		sr_toggle( SRB_U, bitvalue(_ab,msb) == bitvalue(_ab,lsb) );
		*/
	}

	void JitOps::ccr_e_update(const JitReg64& _alu) const
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

			m_asm.mov(mask, asmjit::Imm(0x3fe));

			{
				const PushGP s0s1(m_block, asmjit::x86::rcx);

				sr_getBitValue(s0s1, SRB_S0);
				m_asm.shl(mask, s0s1.get().r8());
				sr_getBitValue(s0s1, SRB_S1);
				m_asm.shr(mask, s0s1.get().r8());
			}

			m_asm.and_(mask, asmjit::Imm(0x3ff));

			{
				const RegGP alu(m_block);
				m_asm.mov(alu, _alu);
				m_asm.shr(alu, asmjit::Imm(46));
				m_asm.and_(alu, mask.get());

				// res = alu != mask && alu != 0

				// Don't be distracted by the names, we abuse alu & mask here to store comparison results
				m_asm.cmp(alu, mask.get());			m_asm.setnz(mask);
				m_asm.cmp(alu, asmjit::Imm(0));		m_asm.setnz(alu);

				m_asm.and_(mask.get().r8(), alu.get().r8());
			}

			ccr_update(mask, CCRB_E);
		}
	}

	void JitOps::ccr_n_update_by55(const JitReg64& _alu) const
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bt(_alu, asmjit::Imm(55));
		ccr_update_ifCarry(CCRB_N);
	}

	inline void JitOps::ccr_n_update_by47(const JitReg64& _alu) const
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bt(_alu, asmjit::Imm(47));
		ccr_update_ifCarry(CCRB_N);
	}

	inline void JitOps::ccr_n_update_by23(const JitReg64& _alu) const
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bt(_alu, asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
	}

	void JitOps::ccr_s_update(const JitReg64& _alu) const
	{
		const auto exit = m_asm.newLabel();

		m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_S));
		m_asm.jc(exit);

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
			m_asm.setc(bit46);

			m_asm.dec(bit);
			m_asm.bt(_alu, bit.get());
			m_asm.setc(bit);
			m_asm.xor_(bit,bit46.get());
		}

		ccr_update_ifNotZero(CCRB_S);

		m_asm.bind(exit);
	}

	inline void JitOps::ccr_l_update_by_v() const
	{
		RegGP r(m_block);
		m_asm.xor_(r,r.get());
		sr_getBitValue(r, CCRB_V);
		m_asm.shl(r, CCRB_L);
		m_asm.or_(regSR, r.get());
	}

	inline void JitOps::ccr_v_update(const JitReg64& _nonMaskedResult) const
	{
		{
			const RegGP signextended(m_block);
			m_asm.mov(signextended, _nonMaskedResult);
			signextend56to64(signextended);
			m_asm.cmp(signextended, _nonMaskedResult);
		}

		ccr_update_ifNotZero(CCRB_V);
		ccr_l_update_by_v();
	}
}
