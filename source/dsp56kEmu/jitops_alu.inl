#pragma once

#include "jitops.h"

namespace dsp56k
{
	constexpr int64_t g_alu_max_56		=  0x7FFFFFFFFFFFFF;
	constexpr int64_t g_alu_min_56		= -0x80000000000000;
	constexpr uint64_t g_alu_max_56_u	=  0xffffffffffffff;

	inline void JitOps::op_Abs(TWord op)
	{
		const RegGP ra(m_block.gpPool());

		const auto ab = getFieldValue<Abs, Field_d>(op);

		m_dspRegs.getALU(ra, ab ? 1 : 0);						// Load ALU
		signextend56to64(ra);									// extend to 64 bits

		{
			const RegGP rb(m_block.gpPool());
			m_asm.mov(rb, static_cast<asmjit::x86::Gp>(ra));	// Copy to backup location

			m_asm.neg(ra);										// negate

			m_asm.cmovl(ra, rb);								// if tempA is now negative, restore its saved value
		}

//		m_asm.and_(ra, asmjit::Imm(0x00ff ffffff ffffff));		// absolute value does not need any mask

		m_dspRegs.setALU(ab ? 1 : 0, ra);						// Store ALU

		ccr_update_ifZero(SRB_Z);

	//	sr_v_update(d);
	//	sr_l_update_by_v();
		m_srDirty = true;
		ccr_dirty(ra);
	}
	
	inline void JitOps::alu_add(TWord ab, RegGP& _v)
	{
		const RegGP alu(m_block.gpPool());
		
		m_dspRegs.getALU(alu, ab ? 1 : 0);

		m_asm.add(alu, _v.get());

		_v.release();

		{
			const RegGP aluMax(m_block.gpPool());
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(alu, aluMax.get());

		}

		ccr_update_ifGreater(SRB_C);

		ccr_clear(SR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops

		mask56(alu);
		ccr_update_ifZero(SRB_Z);

		// S L E U N Z V C

//		sr_l_update_by_v();

		ccr_dirty(alu);

		m_dspRegs.setALU(ab ? 1 : 0, alu);
	}

	void JitOps::op_Add_SD(TWord op)
	{
		const auto D = getFieldValue<Add_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Add_SD, Field_JJJ>(op);

		RegGP v(m_block.gpPool());
		decode_JJJ_read_56(v, JJJ, !D);
		alu_add(D, v);
	}
}
