#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "jittypes.h"

namespace dsp56k
{
	inline void JitOps::ccr_clear(CCRMask _mask)
	{
		m_asm.and_(m_dspRegs.getSR(), asmjit::Imm(~_mask));
	}

	inline void JitOps::ccr_set(CCRMask _mask)
	{
		m_asm.or_(m_dspRegs.getSR(), asmjit::Imm(_mask));
	}

	inline void JitOps::ccr_dirty(const asmjit::x86::Gpq& _alu)
	{
		m_asm.movq(regLastModAlu, _alu);
	}

	inline void JitOps::ccr_update_ifZero(CCRBit _bit)
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setz(ra);										// set reg to 1 if last operation returned zero, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update_ifGreater(CCRBit _bit)
	{
		const RegGP ra(m_block.gpPool());
		m_asm.setg(ra);										// set reg to 1 if last operation returned >, 0 otherwise
		ccr_update(ra, _bit);
	}

	inline void JitOps::ccr_update(const RegGP& ra, CCRBit _bit)
	{
		m_asm.and_(ra, asmjit::Imm(0xff));
		ccr_clear(static_cast<CCRMask>(1 << _bit));			// clear out old status register value
		m_asm.shl(ra, _bit);								// shift left to become our new SR bit
		m_asm.or_(m_dspRegs.getSR(), ra.get());				// or in our new SR bit
	}

}
