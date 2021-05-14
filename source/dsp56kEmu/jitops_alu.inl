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

		m_dspRegs.mask56(alu);
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

	inline void JitOps::alu_and(const TWord ab, RegGP& _v) const
	{
		m_asm.shl(_v, asmjit::Imm(24));
		ccr_update_ifZero(SRB_Z);

		const AluReg alu(m_block, ab);

		{
			const RegGP mask(m_block);

			m_asm.mov(mask, asmjit::Imm(0xff000000ffffff));
			m_asm.and_(alu, mask.get());
			m_asm.or_(alu, _v.get());
		}

		_v.release();
		
		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(alu);
		ccr_clear(SR_V);
		ccr_s_update(alu);
	}

	inline void JitOps::alu_asl(TWord abSrc, TWord abDst, const PushGP& _v)
	{
		const AluReg alu(m_block, abSrc, false, abDst);

		m_asm.sal(alu, asmjit::Imm(8));				// we want to hit the 64 bit boundary to make use of the native carry flag so pre-shift by 8 bit (56 => 64)

		m_asm.sal(alu, _v.get());					// now do the real shift

		ccr_update_ifCarry(SRB_C);					// copy the host carry flag to the DSP carry flag
		ccr_update_ifZero(SRB_Z);					// we can check for zero now, too

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical ot the backshifted one
		{
			const AluReg oldAlu(m_block, abSrc, true);
			m_asm.sal(oldAlu, asmjit::Imm(8));
			m_asm.sar(alu, _v.get());
			m_asm.cmp(alu, oldAlu.get());
		}

		ccr_update_ifZero(SRB_V);

		m_asm.sal(alu, _v.get());					// one more time
		m_asm.shr(alu, asmjit::Imm(8));				// correction

		// S L E U N Z V C
		ccr_dirty(alu);
	}

	inline void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const PushGP& _v)
	{
		const AluReg alu(m_block, _abSrc, false, _abDst);

		m_asm.sar(alu, _v.get());

		ccr_update_ifCarry(SRB_C);					// copy the host carry flag to the DSP carry flag
		ccr_update_ifZero(SRB_Z);					// we can check for zero now, too
		ccr_clear(SR_V);

		// S L E U N Z V C
		ccr_dirty(alu);
	}

	inline void JitOps::errNotImplemented(TWord op)
	{
		assert(0 && "instruction not implemented");
	}

	inline void JitOps::op_Add_SD(TWord op)
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
		getOpWordB(r);
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

		m_dspRegs.mask56(aluD);

		ccr_update_ifZero(SRB_Z);
		ccr_clear(SR_V);	// TODO: Set if overflow has occurred in the A or B result or the MSB of the destination operand is changed as a result of the instruction’s left shift.
		ccr_dirty(aluD);
	}

	inline void JitOps::op_Addr(TWord op)
	{
		// D = D/2 + S

		const auto ab = getFieldValue<Addl, Field_d>(op);

		const AluReg aluD(m_block, ab);

		m_asm.sal(aluD, asmjit::Imm(8));
		m_asm.sar(aluD, asmjit::Imm(1));
		m_asm.shr(aluD, asmjit::Imm(8));

		{
			const AluReg aluS(m_block, ab ? 0 : 1, true);
			m_asm.add(aluD, aluS.get());
		}

		{
			const RegGP aluMax(m_block);
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(aluD, aluMax.get());
		}

		ccr_update_ifGreater(SRB_C);

		m_dspRegs.mask56(aluD);

		ccr_update_ifZero(SRB_Z);
		ccr_clear(SR_V);			// TODO: Changed according to the standard definition.
		ccr_dirty(aluD);
	}

	inline void JitOps::op_And_SD(TWord op)
	{
		const auto D = getFieldValue<And_SD, Field_d>(op);
		const auto JJ = getFieldValue<And_SD, Field_JJ>(op);

		RegGP r(m_block);
		decode_JJ_read(r, JJ);
		alu_and(D, r);
	}

	inline void JitOps::op_And_xx(TWord op)
	{
		const auto ab	= getFieldValue<And_xx,Field_d>(op);
		const auto xxxx	= getFieldValue<And_xx,Field_iiiiii>(op);

		RegGP r(m_block);
		m_asm.mov(r, asmjit::Imm(xxxx));
		alu_and(ab,r);
	}

	inline void JitOps::op_And_xxxx(TWord op)
	{
		const auto ab = getFieldValue<And_xxxx,Field_d>(op);

		RegGP r(m_block);
		getOpWordB(r);
		alu_and(ab,r);
	}

	inline void JitOps::op_Andi(TWord op)
	{
		const auto ee		= getFieldValue<Andi,Field_EE>(op);
		const auto iiiiii	= getFieldValue<Andi,Field_iiiiiiii>(op);

		RegGP r(m_block);
		decode_EE_read(r, ee);
		m_asm.and_(r, asmjit::Imm(iiiiii));
		decode_EE_write(r, ee);
	}

	inline void JitOps::op_Asl_D(TWord op)
	{
		const auto D = getFieldValue<Asl_D, Field_d>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const PushGP r(m_block, asmjit::x86::rcx);
		m_asm.mov(r, asmjit::Imm(1));
		alu_asl(D, D, r);
	}

	inline void JitOps::op_Asl_ii(TWord op)
	{
		const TWord shiftAmount	= getFieldValue<Asl_ii,Field_iiiiii>(op);

		const bool abDst		= getFieldValue<Asl_ii,Field_D>(op);
		const bool abSrc		= getFieldValue<Asl_ii,Field_S>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const PushGP r(m_block, asmjit::x86::rcx);
		m_asm.mov(r, asmjit::Imm(shiftAmount));
		alu_asl(abSrc, abDst, r);
	}

	inline void JitOps::op_Asl_S1S2D(TWord op)
	{
		const auto sss   = getFieldValue<Asl_S1S2D,Field_sss>(op);
		const bool abDst = getFieldValue<Asl_S1S2D,Field_D>(op);
		const bool abSrc = getFieldValue<Asl_S1S2D,Field_S>(op);

		const PushGP r(m_block, asmjit::x86::rcx);
		decode_sss_read( r.get(), sss );
		m_asm.and_(r, asmjit::Imm(0x3f));	// "In the control register S1: bits 5–0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		alu_asl(abDst, abSrc, r);
	}

	inline void JitOps::op_Asr_D(TWord op)
	{
		const auto D = getFieldValue<Asr_D, Field_d>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const PushGP r(m_block, asmjit::x86::rcx);
		m_asm.mov(r, asmjit::Imm(1));
		alu_asr(D, D, r);
	}

	inline void JitOps::op_Asr_ii(TWord op)
	{
		const TWord shiftAmount	= getFieldValue<Asr_ii,Field_iiiiii>(op);

		const bool abDst		= getFieldValue<Asr_ii,Field_D>(op);
		const bool abSrc		= getFieldValue<Asr_ii,Field_S>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const PushGP r(m_block, asmjit::x86::rcx);
		m_asm.mov(r, asmjit::Imm(shiftAmount));
		alu_asr(abSrc, abDst, r);
	}

	inline void JitOps::op_Asr_S1S2D(TWord op)
	{
		const auto sss   = getFieldValue<Asr_S1S2D,Field_sss>(op);
		const bool abDst = getFieldValue<Asr_S1S2D,Field_D>(op);
		const bool abSrc = getFieldValue<Asr_S1S2D,Field_S>(op);

		const PushGP r(m_block, asmjit::x86::rcx);
		decode_sss_read( r.get(), sss );
		m_asm.and_(r, asmjit::Imm(0x3f));	// "In the control register S1: bits 5–0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		alu_asr(abDst, abSrc, r);
	}

	inline void JitOps::alu_bclr(const JitReg64& _dst, const TWord _bit) const
	{
		m_asm.btr(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::alu_bset(const JitReg64& _dst, const TWord _bit) const
	{
		m_asm.bts(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::alu_bchg(const JitReg64& _dst, const TWord _bit) const
	{
		m_asm.btc(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(SRB_C);
	}

	template<Instruction Inst> void JitOps::bitmod_ea(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord) const)
	{
		const auto area = getFieldValueMemArea<Inst>(op);

		const RegGP offset(m_block);
		effectiveAddress<Inst>(offset, op);

		const RegGP regMem(m_block);
		readMemOrPeriph(regMem, area, offset);
		(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
		writeMemOrPeriph(area, offset, regMem);
	}
	
	template<Instruction Inst> void JitOps::bitmod_aa(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord) const)
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto addr = getFieldValue<Inst, Field_aaaaaa>(op);
		const RegGP regMem(m_block);
		m_block.mem().readDspMemory(regMem, area, addr);
		(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
		m_block.mem().writeDspMemory(area, addr, regMem);
	}

	template<Instruction Inst> void JitOps::bitmod_ppqq(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord) const)
	{
		const RegGP r(m_block);
		readMem<Inst>(r, op);
		(this->*_bitmodFunc)(r, getBit<Inst>(op));
		writeMem<Inst>(op, r);
	}

	template<Instruction Inst> void JitOps::bitmod_D(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord) const)
	{
		const auto bit		= getBit<Inst>(op);
		const auto dddddd	= getFieldValue<Inst,Field_DDDDDD>(op);

		const RegGP d(m_block);
		decode_dddddd_read(d.get().r32(), dddddd);
		(this->*_bitmodFunc)(d, getBit<Inst>(op));
		decode_dddddd_write(dddddd, d.get().r32());
	}
	
	inline void JitOps::op_Bchg_ea(TWord op)	{ bitmod_ea<Bclr_ea>(op, &JitOps::alu_bchg); }
	inline void JitOps::op_Bchg_aa(TWord op)	{ bitmod_aa<Bclr_aa>(op, &JitOps::alu_bchg); }
	inline void JitOps::op_Bchg_pp(TWord op)	{ bitmod_ppqq<Bclr_pp>(op, &JitOps::alu_bchg); }
	inline void JitOps::op_Bchg_qq(TWord op)	{ bitmod_ppqq<Bclr_qq>(op, &JitOps::alu_bchg); }
	inline void JitOps::op_Bchg_D(TWord op)		{ bitmod_D<Bclr_D>(op, &JitOps::alu_bchg); }

	inline void JitOps::op_Bclr_ea(TWord op)	{ bitmod_ea<Bclr_ea>(op, &JitOps::alu_bclr); }
	inline void JitOps::op_Bclr_aa(TWord op)	{ bitmod_aa<Bclr_aa>(op, &JitOps::alu_bclr); }
	inline void JitOps::op_Bclr_pp(TWord op)	{ bitmod_ppqq<Bclr_pp>(op, &JitOps::alu_bclr); }
	inline void JitOps::op_Bclr_qq(TWord op)	{ bitmod_ppqq<Bclr_qq>(op, &JitOps::alu_bclr); }
	inline void JitOps::op_Bclr_D(TWord op)		{ bitmod_D<Bclr_D>(op, &JitOps::alu_bclr); }

	inline void JitOps::op_Bset_ea(TWord op)	{ bitmod_ea<Bset_ea>(op, &JitOps::alu_bset); }
	inline void JitOps::op_Bset_aa(TWord op)	{ bitmod_aa<Bset_aa>(op, &JitOps::alu_bset); }
	inline void JitOps::op_Bset_pp(TWord op)	{ bitmod_ppqq<Bset_pp>(op, &JitOps::alu_bset); }
	inline void JitOps::op_Bset_qq(TWord op)	{ bitmod_ppqq<Bset_qq>(op, &JitOps::alu_bset); }
	inline void JitOps::op_Bset_D(TWord op)		{ bitmod_D<Bset_D>(op, &JitOps::alu_bset); }

	inline void JitOps::op_Btst_ea(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_ea>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_ea>(op)));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::op_Btst_aa(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_aa>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_aa>(op)));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::op_Btst_pp(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_pp>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_pp>(op)));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::op_Btst_qq(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_qq>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_qq>(op)));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::op_Btst_D(TWord op)
	{
		const auto dddddd	= getFieldValue<Btst_D,Field_DDDDDD>(op);
		const auto bit		= getBit<Btst_D>(op);

		const RegGP r(m_block);
		decode_dddddd_read(r.get().r32(), dddddd);

		m_asm.bt(r.get().r32(), asmjit::Imm(bit));
		ccr_update_ifCarry(SRB_C);
	}

	inline void JitOps::op_Clr(TWord op)
	{
		const auto D = getFieldValue<Clr, Field_d>(op);
		const auto xm = regAlu(D);
		m_asm.pxor(xm, xm);
		ccr_clear( static_cast<CCRMask>(SR_E | SR_N | SR_V) );
		ccr_set( static_cast<CCRMask>(SR_U | SR_Z) );
		m_ccrDirty = false;
	}

	inline void JitOps::op_Ori(TWord op)
	{
		const TWord ee		= getFieldValue<Ori,Field_EE>(op);
		const TWord iiiiii	= getFieldValue<Ori,Field_iiiiiiii>(op);

		RegGP r(m_block);
		decode_EE_read(r, ee);
		m_asm.or_(r, asmjit::Imm(iiiiii));
		decode_EE_write(r, ee);
	}
}
