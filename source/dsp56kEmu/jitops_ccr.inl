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
}
