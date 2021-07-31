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
}
