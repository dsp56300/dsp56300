#pragma once

#include "jitblock.h"
#include "jitops.h"
#include "jitregtypes.h"

namespace dsp56k
{
	constexpr bool g_useSRCache = true;

	inline void JitOps::ccr_set(CCRMask _mask)
	{
		m_asm.or_(r32(m_dspRegs.getSR(JitDspRegs::ReadWrite)), asmjit::Imm(_mask));
		ccr_clearDirty(_mask);
	}

	inline void JitOps::ccr_dirty(TWord _aluIndex, const JitReg64& _alu, CCRMask _dirtyBits)
	{
		if(g_useSRCache)
		{
			// if the last dirty call marked bits as dirty that are no longer to be dirtied now, we need to update them
			const auto lastDirty = m_ccrDirty & ~_dirtyBits;
			updateDirtyCCR(static_cast<CCRMask>(lastDirty));

			m_asm.movq(regLastModAlu, _alu);

			m_ccrDirty = static_cast<CCRMask>(m_ccrDirty | _dirtyBits);
		}
		else
		{
			updateDirtyCCR(_alu, _dirtyBits);
		}
	}

	void JitOps::ccr_clearDirty(const CCRMask _mask)
	{
		m_ccrDirty = static_cast<CCRMask>(m_ccrDirty & ~_mask);
	}

	void JitOps::updateDirtyCCR()
	{
		if(!m_ccrDirty)
			return;

		updateDirtyCCR(m_ccrDirty);
	}

	void JitOps::updateDirtyCCR(CCRMask _whatToUpdate)
	{
		const auto dirty = m_ccrDirty & _whatToUpdate;
		if(!dirty)
			return;

		const RegGP r(m_block);
		m_asm.movq(r, regLastModAlu);
		updateDirtyCCR(r, static_cast<CCRMask>(dirty));
	}

	inline void JitOps::updateDirtyCCR(const JitReg64& _alu, CCRMask _dirtyBits)
	{
		m_ccr_update_clear = false;

#ifdef HAVE_ARM64
		m_asm.mov(r32(regReturnVal), asmjit::Imm(~_dirtyBits));
		m_asm.and_(r32(m_dspRegs.getSR(JitDspRegs::ReadWrite)), r32(regReturnVal));
#else
		m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(~_dirtyBits));
#endif

		if(_dirtyBits & CCR_V)
		{
			ccr_v_update(_alu);
			m_dspRegs.mask56(_alu);
		}
		if(_dirtyBits & CCR_Z)
		{
			m_asm.test(_alu);
			ccr_update_ifZero(CCRB_Z);
		}
		if(_dirtyBits & CCR_N)
			ccr_n_update_by55(_alu);
		if(_dirtyBits & CCR_E)
			ccr_e_update(_alu);
		if(_dirtyBits & CCR_U)
			ccr_u_update(_alu);

		m_ccr_update_clear = true;
	}

	inline void JitOps::ccr_v_update(const JitReg64& _nonMaskedResult)
	{
		{
			const auto signextended = regReturnVal;
			m_asm.mov(signextended, _nonMaskedResult);
			signextend56to64(signextended);
			m_asm.cmp(signextended, _nonMaskedResult);
		}

		ccr_update_ifNotZero(CCRB_V);
		ccr_l_update_by_v();
	}
}
