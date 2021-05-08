#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "jittypes.h"

namespace dsp56k
{
	inline void JitOps::ccr_clear(CCRMask _mask) const
	{
		m_asm.and_(m_dspRegs.getSR(), asmjit::Imm(~_mask));
	}

	inline void JitOps::ccr_set(CCRMask _mask) const
	{
		m_asm.or_(m_dspRegs.getSR(), asmjit::Imm(_mask));
	}

	inline void JitOps::ccr_dirty(const asmjit::x86::Gpq& _alu)
	{
		m_asm.movq(regLastModAlu, _alu);
		m_srDirty = true;
	}

	inline void JitOps::sr_getBitValue(const asmjit::x86::Gpq& _dst, CCRBit _bit) const
	{
		m_asm.bt(regSR, asmjit::Imm(_bit));
		m_asm.setc(_dst);
	}

	inline void JitOps::ccr_update_ifZero(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setz(ra);										// set reg to 1 if last operation returned zero, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreater(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setg(ra);										// set reg to 1 if last operation returned >, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreaterEqual(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setge(ra);									// set reg to 1 if last operation returned >=, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifLessThan(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setl(ra);										// set reg to 1 if last operation returned <, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifLessEqual(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setle(ra);									// set reg to 1 if last operation returned <=, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifCarry(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setc(ra);										// set reg to 1 if last operation generated carry, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifParity(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setp(ra);										// set reg to 1 if number of 1 bits is even, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotParity(CCRBit _bit) const
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setnp(ra);									// set reg to 1 if number of 1 bits is odd, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update(const RegGP& ra, CCRBit _bit) const
	{
		m_asm.and_(ra, asmjit::Imm(0xff));
		ccr_clear(static_cast<CCRMask>(1 << _bit));			// clear out old status register value
		m_asm.shl(ra, _bit);								// shift left to become our new SR bit
		m_asm.or_(m_dspRegs.getSR(), ra.get());				// or in our new SR bit
	}

	void JitOps::ccr_u_update(const RegGP& _alu) const
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
			m_asm.mov(r,_alu.get());
			m_asm.shr(r, asmjit::Imm(32));	// thx to intel, we are only allowed to shift 32 at max. Therefore, we need to split it
			m_asm.shr(r, shift.get().r8());
			m_asm.and_(r, asmjit::Imm(0x3));
		}
		ccr_update_ifParity(SRB_U);

		/*
		const auto sOffset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);

		const auto msb = 47 + sOffset;
		const auto lsb = 46 + sOffset;

		sr_toggle( SRB_U, bitvalue(_ab,msb) == bitvalue(_ab,lsb) );
		*/
	}

	void JitOps::ccr_e_update(const RegGP& _alu) const
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
				m_asm.mov(alu, _alu.get());
				m_asm.shr(alu, asmjit::Imm(46));
				m_asm.and_(alu, mask.get());

				// res = alu != mask && alu != 0

				// Don't be distracted by the names, we abuse alu & mask here to store comparison results
				m_asm.cmp(alu, mask.get());			m_asm.setnz(mask);
				m_asm.cmp(alu, asmjit::Imm(0));		m_asm.setnz(alu);

				m_asm.and_(mask.get().r8(), alu.get().r8());
			}

			ccr_update(mask, SRB_E);
		}

	}

	void JitOps::ccr_n_update(const RegGP& _alu) const
	{
		// Negative
		// Set if the MSB of the result is set; otherwise, this bit is cleared.
		m_asm.bt(_alu, asmjit::Imm(55));
		ccr_update_ifCarry(SRB_N);
	}
}
