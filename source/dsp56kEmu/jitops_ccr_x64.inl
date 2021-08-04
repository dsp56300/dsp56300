#pragma once

#include "jitdspregs.h"
#include "jitops.h"

#include "asmjit/core/operand.h"

namespace dsp56k
{
	inline void JitOps::ccr_getBitValue(const JitRegGP& _dst, CCRBit _bit)
	{
		updateDirtyCCR(static_cast<CCRMask>(1 << _bit));
		m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(_bit));
		m_asm.setc(_dst);
	}

	inline void JitOps::sr_getBitValue(const JitRegGP& _dst, SRBit _bit) const
	{
		m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(_bit));
		m_asm.setc(_dst);
	}

	inline void JitOps::ccr_update_ifZero(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setz(ra);										// set reg to 1 if last operation returned zero, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotZero(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setnz(ra);									// set reg to 1 if last operation returned != 0, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreater(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setg(ra);										// set reg to 1 if last operation returned >, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreaterEqual(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setge(ra);									// set reg to 1 if last operation returned >=, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifLess(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setl(ra);										// set reg to 1 if last operation returned <, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifLessEqual(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setle(ra);									// set reg to 1 if last operation returned <=, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifCarry(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setc(ra);										// set reg to 1 if last operation generated carry, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotCarry(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setnc(ra);									// set reg to 1 if last operation did NOT generate carry, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifParity(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setp(ra);										// set reg to 1 if number of 1 bits is even, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifNotParity(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setnp(ra);									// set reg to 1 if number of 1 bits is odd, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifAbove(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.seta(ra);
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifBelow(CCRBit _bit)
	{
		const RegGP ra(m_block);
		m_asm.setb(ra);
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update(const JitRegGP& ra, CCRBit _bit)
	{
		const auto mask = static_cast<CCRMask>(1 << _bit);

		if (m_ccr_update_clear && _bit != CCRB_L && _bit != CCRB_S)
			ccr_clear(mask);												// clear out old status register value
		else
			ccr_clearDirty(mask);

		if (_bit)
			m_asm.shl(ra.r8(), _bit);										// shift left to become our new SR bit
		m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite).r8(), ra.r8());	// or in our new SR bit
	}
}
