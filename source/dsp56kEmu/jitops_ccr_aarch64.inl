#pragma once

namespace dsp56k
{
	inline void JitOps::ccr_getBitValue(const JitRegGP& _dst, CCRBit _bit)
	{
		updateDirtyCCR(static_cast<CCRMask>(1 << _bit));
		sr_getBitValue(_dst, static_cast<SRBit>(_bit));
	}

	inline void JitOps::sr_getBitValue(const JitRegGP& _dst, SRBit _bit) const
	{
		m_asm.ubfx(_dst, m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(_bit), 1);
	}

	inline void JitOps::ccr_update_ifZero(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kZero);
	}

	inline void JitOps::ccr_update_ifNotZero(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kNotZero);
	}

	inline void JitOps::ccr_update_ifGreater(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kGT);
	}

	inline void JitOps::ccr_update_ifGreaterEqual(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kGE);
	}

	inline void JitOps::ccr_update_ifLess(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kLT);
	}

	inline void JitOps::ccr_update_ifLessEqual(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kLE);
	}

	inline void JitOps::ccr_update_ifCarry(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kCS);
	}

	inline void JitOps::ccr_update_ifNotCarry(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kCC);
	}

	inline void JitOps::ccr_update_ifAbove(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kHI);
	}

	inline void JitOps::ccr_update_ifBelow(CCRBit _bit)
	{
		ccr_update(_bit, asmjit::arm::Cond::kLO);
	}

	void JitOps::ccr_update(CCRBit _bit, uint32_t _armConditionCode)
	{
		const RegGP ra(m_block);
		m_asm.cset(ra, _armConditionCode);
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update(const JitRegGP& ra, CCRBit _bit)
	{
		const auto mask = static_cast<CCRMask>(1 << _bit);

		if (m_ccr_update_clear && _bit != CCRB_L && _bit != CCRB_S)
			ccr_clear(mask);												// clear out old status register value
		else
			ccr_clearDirty(mask);

		m_asm.bfi(m_dspRegs.getSR(JitDspRegs::ReadWrite), ra, asmjit::Imm(_bit), asmjit::Imm(1));
	}
}
