#pragma once

#include "jitops.h"

namespace dsp56k
{
	constexpr int64_t g_alu_max_56		=  0x7FFFFFFFFFFFFF;
	constexpr int64_t g_alu_min_56		= -0x80000000000000;
	constexpr uint64_t g_alu_max_56_u	=  0xffffffffffffff;

	inline void JitOps::op_Abs(TWord op)
	{
		const auto ab = getFieldValue<Abs, Field_d>(op);

		const AluReg ra(m_block, ab);							// Load ALU

		signextend56to64(ra);									// extend to 64 bits

		{
			const RegGP rb(m_block);
			m_asm.mov(rb, static_cast<asmjit::x86::Gp>(ra));	// Copy to backup location

			m_asm.neg(ra);										// negate

			m_asm.cmovl(ra, rb);								// if tempA is now negative, restore its saved value
		}

//		m_asm.and_(ra, asmjit::Imm(0x00ff ffffff ffffff));		// absolute value does not need any mask

		ccr_update_ifZero(SRB_Z);

	//	sr_v_update(d);
	//	sr_l_update_by_v();
		ccr_dirty(ra);
	}

	inline void JitOps::op_ADC(TWord op)
	{
		assert(false && "not implemented");
	}

	inline void JitOps::alu_add(TWord ab, RegGP& _v)
	{
		const AluReg alu(m_block, ab);

		m_asm.add(alu, _v.get());

		_v.release();

		{
			const RegGP aluMax(m_block);
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(alu, aluMax.get());
		}

		ccr_update_ifGreater(SRB_C);

		ccr_clear(SR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops

		mask56(alu);
		ccr_update_ifZero(SRB_Z);

//		sr_l_update_by_v();

		ccr_dirty(alu);
	}

	inline void JitOps::unsignedImmediateToAlu(const RegGP& _r, const asmjit::Imm& _i) const
	{
		m_asm.mov(_r, _i);
		m_asm.shl(_r, asmjit::Imm(24));
	}

	inline void JitOps::alu_add(TWord ab, const asmjit::Imm& _v)
	{
		RegGP r(m_block);
		unsignedImmediateToAlu(r, _v);
		alu_add(ab, r);
	}

	void JitOps::op_Add_SD(TWord op)
	{
		const auto D = getFieldValue<Add_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Add_SD, Field_JJJ>(op);

		RegGP v(m_block);
		decode_JJJ_read_56(v, JJJ, !D);
		alu_add(D, v);
	}

	inline void JitOps::op_Add_xx(TWord op)
	{
		const auto iiiiii	= getFieldValue<Add_xx,Field_iiiiii>(op);
		const auto ab		= getFieldValue<Add_xx,Field_d>(op);

		alu_add( ab, asmjit::Imm(iiiiii) );
	}

	inline void JitOps::op_Add_xxxx(TWord op)
	{
		const auto ab = getFieldValue<Add_xxxx,Field_d>(op);

		RegGP r(m_block);
		m_block.mem().getOpWordB(r);
		signed24To56(r);
		alu_add(ab, r);
	}

	inline void JitOps::op_Addl(TWord op)
	{
		// D = 2 * D + S

		const auto ab = getFieldValue<Addl, Field_d>(op);

		const AluReg aluD(m_block, ab);

		signextend56to64(aluD);

		m_asm.sal(aluD, asmjit::Imm(1));

		{
			const AluReg aluS(m_block, ab ? 0 : 1, true);

			signextend56to64(aluS);

			m_asm.add(aluD, aluS.get());
		}

		ccr_update_ifCarry(SRB_C);

		mask56(aluD);

		ccr_update_ifZero(SRB_Z);
		ccr_clear(SR_Z);
		ccr_dirty(aluD);
	}
}
