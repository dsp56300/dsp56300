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
}
