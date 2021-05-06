#pragma once

#include "jitops.h"

namespace dsp56k
{
	inline void JitOps::signextend56to64(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
	}

	inline void JitOps::ccr_z_update()
	{
		ccr_clear(SR_Z);											// clear out old Z value
		m_asm.setz(regGPTempA);										// set reg to 1 if last operation returned zero, 0 otherwise
		m_asm.shl(regGPTempA, SRB_Z);								// shift left to become our Z
		m_asm.or_(m_dspRegs.getSR(), regGPTempA);					// or in our new Z
	}

	void JitOps::alu_abs(bool ab)
	{
		m_dspRegs.getALU(regGPTempA, ab ? 1 : 0);					// Load ALU and extend to 64 bits
		signextend56to64(regGPTempA);

		m_asm.mov(regGPTempB, regGPTempA);							// Copy to backup location

		m_asm.neg(regGPTempA);										// negate

		m_asm.cmovl(regGPTempA, regGPTempB);						// if tempA is now negative, restore its saved value

//		m_asm.and_(regGPTempA, asmjit::Imm(0x00ff ffffff ffffff));	// absolute value does not need any mask

		m_dspRegs.setALU(ab ? 1 : 0, regGPTempA);					// Store ALU

		ccr_z_update();

	//	sr_v_update(d);
	//	sr_l_update_by_v();
		m_srDirty = true;
		ccr_dirty(regGPTempA);
	}
}
