#pragma once

#include "jitops.h"

#include "asmjit/x86/x86features.h"

namespace dsp56k
{
	constexpr int64_t g_alu_max_56		=  0x7FFFFFFFFFFFFF;
	constexpr int64_t g_alu_min_56		= -0x80000000000000;
	constexpr uint64_t g_alu_max_56_u	=  0xffffffffffffff;

	void JitOps::XYto56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		signextend48to56(_dst);
	}

	void JitOps::XY0to56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shl(_dst, asmjit::Imm(40));
		m_asm.sar(_dst, asmjit::Imm(8));
		m_asm.shr(_dst, asmjit::Imm(8));
	}

	void JitOps::XY1to56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shr(_dst, asmjit::Imm(24));	// remove LSWord
		signed24To56(_dst);
	}

	inline void JitOps::op_Abs(TWord op)
	{
		const auto ab = getFieldValue<Abs, Field_d>(op);

		AluRef ra(m_block, ab);							// Load ALU

		signextend56to64(ra);								// extend to 64 bits

		alu_abs(ra);

		m_dspRegs.mask56(ra);

	//	sr_v_update(d);
	//	sr_l_update_by_v();
		ccr_dirty(ab, ra, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_add(const TWord _ab, RegGP& _v)
	{
		AluRef alu(m_block, _ab);

		m_asm.add(alu, _v.get());

		_v.release();

		{
			const auto aluMax = regReturnVal;
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(alu, aluMax);
		}

		ccr_update_ifAbove(CCRB_C);

		ccr_clear(CCR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops

		m_dspRegs.mask56(alu);

//		sr_l_update_by_v();

		ccr_dirty(_ab, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_add(const TWord _ab, const asmjit::Imm& _v)
	{
		RegGP r(m_block);
		unsignedImmediateToAlu(r, _v);
		alu_add(_ab, r);
	}

	inline void JitOps::alu_sub(const TWord _ab, RegGP& _v)
	{
		AluRef alu(m_block, _ab);

		m_asm.sub(alu, _v.get());

		_v.release();

		{
			const auto aluMax = regReturnVal;
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(alu, aluMax);
		}

		ccr_update_ifAbove(CCRB_C);

		ccr_clear(CCR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops

		m_dspRegs.mask56(alu);

//		sr_l_update_by_v();

		ccr_dirty(_ab, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_sub(const TWord _ab, const asmjit::Imm& _v)
	{
		RegGP r(m_block);
		unsignedImmediateToAlu(r, _v);
		alu_sub(_ab, r);
	}

	inline void JitOps::unsignedImmediateToAlu(const JitReg64& _r, const asmjit::Imm& _i) const
	{
		m_asm.mov(_r, _i);
		m_asm.shl(_r, asmjit::Imm(24));
	}

	inline void JitOps::alu_abs(const JitRegGP& _r)
	{
		const auto rb = regReturnVal;

		m_asm.mov(rb, _r);		// Copy to backup location
		m_asm.neg(_r);			// negate
		m_asm.cmovl(_r, rb);	// if now negative, restore its saved value
	}

	inline void JitOps::alu_and(const TWord ab, RegGP& _v)
	{
		m_asm.shl(_v, asmjit::Imm(24));

		AluRef alu(m_block, ab);

		{
			const RegGP r(m_block);
			m_asm.mov(r, alu.get());
			m_asm.and_(r, _v.get());
			ccr_update_ifZero(CCRB_Z);
		}

		{
			const auto mask = regReturnVal;
			m_asm.mov(mask, asmjit::Imm(0xff000000ffffff));
			m_asm.or_(_v, mask);
			m_asm.and_(alu, _v.get());
		}

		_v.release();
		
		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(alu);
		ccr_clear(CCR_V);
	}

	inline void JitOps::alu_asl(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluReg alu(m_block, _abDst, false, _abDst != _abSrc);
		if(_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.sal(alu, asmjit::Imm(8));				// we want to hit the 64 bit boundary to make use of the native carry flag so pre-shift by 8 bit (56 => 64)

		m_asm.sal(alu, _v.get());					// now do the real shift

		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical ot the backshifted one
		{
			AluReg oldAlu(m_block, _abSrc, true);
			m_asm.sal(oldAlu, asmjit::Imm(8));
			m_asm.sar(alu, _v.get());
			m_asm.cmp(alu, oldAlu.get());
		}

		ccr_update_ifNotZero(CCRB_V);

		m_asm.sal(alu, _v.get());					// one more time
		m_asm.shr(alu, asmjit::Imm(8));				// correction

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if(_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.sal(alu, asmjit::Imm(8));
		m_asm.sar(alu, _v.get());
		m_asm.sar(alu, asmjit::Imm(8));
		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag
		m_dspRegs.mask56(alu);

		ccr_clear(CCR_V);

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::jmp(const JitReg32& _absAddr)
	{
		m_dspRegs.setPC(_absAddr);
	}

	inline void JitOps::jsr(const JitReg32& _absAddr)
	{
		pushPCSR();
		jmp(_absAddr);

		if(m_fastInterrupt)
		{
			const auto sr = m_dspRegs.getSR(JitDspRegs::ReadWrite);
			m_asm.and_(sr, asmjit::Imm(~(SR_S1 | SR_S0 | SR_SA | SR_LF)));
			setDspProcessingMode(DSP::LongInterrupt);			
		}
	}

	inline void JitOps::jmp(TWord _absAddr)
	{
		const RegGP r(m_block);
		m_asm.mov(r, asmjit::Imm(_absAddr));
		jmp(r.get().r32());
	}

	inline void JitOps::jsr(const TWord _absAddr)
	{
		const RegGP r(m_block);
		m_asm.mov(r.get().r32(), asmjit::Imm(_absAddr));
		jsr(r.get().r32());
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
		getOpWordB(r.get());
		signed24To56(r);
		alu_add(ab, r);
	}

	inline void JitOps::op_Addl(TWord op)
	{
		// D = 2 * D + S

		const auto ab = getFieldValue<Addl, Field_d>(op);

		AluReg aluD(m_block, ab);

		signextend56to64(aluD);

		m_asm.sal(aluD, asmjit::Imm(1));

		{
			AluReg aluS(m_block, ab ? 0 : 1, true);

			signextend56to64(aluS);

			m_asm.add(aluD, aluS.get());
		}

		ccr_update_ifCarry(CCRB_C);

		m_dspRegs.mask56(aluD);

		ccr_clear(CCR_V);	// TODO: Set if overflow has occurred in the A or B result or the MSB of the destination operand is changed as a result of the instruction’s left shift.
		ccr_dirty(ab, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Addr(TWord op)
	{
		// D = D/2 + S

		const auto ab = getFieldValue<Addl, Field_d>(op);

		AluReg aluD(m_block, ab);

		m_asm.sal(aluD, asmjit::Imm(8));
		m_asm.sar(aluD, asmjit::Imm(1));
		m_asm.shr(aluD, asmjit::Imm(8));

		{
			AluRef aluS(m_block, ab ? 0 : 1, true, false);
			m_asm.add(aluD, aluS.get());
		}

		{
			const auto aluMax = regReturnVal;
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(aluD, aluMax);
		}

		ccr_update_ifGreater(CCRB_C);

		m_dspRegs.mask56(aluD);

		ccr_clear(CCR_V);			// TODO: Changed according to the standard definition.
		ccr_dirty(ab, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
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
		const ShiftReg r(m_block);
		m_asm.mov(r, asmjit::Imm(1));
		alu_asl(D, D, r);
	}

	inline void JitOps::op_Asl_ii(TWord op)
	{
		const TWord shiftAmount	= getFieldValue<Asl_ii,Field_iiiiii>(op);

		const bool abDst		= getFieldValue<Asl_ii,Field_D>(op);
		const bool abSrc		= getFieldValue<Asl_ii,Field_S>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const ShiftReg r(m_block);
		m_asm.mov(r, asmjit::Imm(shiftAmount));
		alu_asl(abSrc, abDst, r);
	}

	inline void JitOps::op_Asl_S1S2D(TWord op)
	{
		const auto sss   = getFieldValue<Asl_S1S2D,Field_sss>(op);
		const bool abDst = getFieldValue<Asl_S1S2D,Field_D>(op);
		const bool abSrc = getFieldValue<Asl_S1S2D,Field_S>(op);

		const ShiftReg r(m_block);
		decode_sss_read( r.get(), sss );
		m_asm.and_(r, asmjit::Imm(0x3f));	// "In the control register S1: bits 5–0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		alu_asl(abDst, abSrc, r);
	}

	inline void JitOps::op_Asr_D(TWord op)
	{
		const auto D = getFieldValue<Asr_D, Field_d>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const ShiftReg r(m_block);
		m_asm.mov(r, asmjit::Imm(1));
		alu_asr(D, D, r);
	}

	inline void JitOps::op_Asr_ii(TWord op)
	{
		const TWord shiftAmount	= getFieldValue<Asr_ii,Field_iiiiii>(op);

		const bool abDst		= getFieldValue<Asr_ii,Field_D>(op);
		const bool abSrc		= getFieldValue<Asr_ii,Field_S>(op);

		// TODO: this is far from optimal, we should use immediate data here
		const ShiftReg r(m_block);
		m_asm.mov(r, asmjit::Imm(shiftAmount));
		alu_asr(abSrc, abDst, r);
	}

	inline void JitOps::op_Asr_S1S2D(TWord op)
	{
		const auto sss   = getFieldValue<Asr_S1S2D,Field_sss>(op);
		const bool abDst = getFieldValue<Asr_S1S2D,Field_D>(op);
		const bool abSrc = getFieldValue<Asr_S1S2D,Field_S>(op);

		const ShiftReg r(m_block);
		decode_sss_read( r.get(), sss );
		m_asm.and_(r, asmjit::Imm(0x3f));	// "In the control register S1: bits 5–0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		alu_asr(abDst, abSrc, r);
	}

	inline void JitOps::alu_bclr(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.btr(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::alu_bset(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.bts(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::alu_bchg(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.btc(_dst, asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::alu_cmp(TWord ab, const JitReg64& _v, bool _magnitude, bool updateCarry/* = true*/)
	{
		AluReg d(m_block, ab, true);

		if(updateCarry)
		{
			m_asm.sal(d.get(), asmjit::Imm(8));
			m_asm.sal(_v, asmjit::Imm(8));			
		}

		if( _magnitude )
		{
			alu_abs(d);
			alu_abs(_v);
		}

		m_asm.sub(d, _v);

		if(updateCarry)
			ccr_update_ifCarry(CCRB_C);

		m_asm.cmp(d, asmjit::Imm(0));

		ccr_clear(CCR_V);			// as cmp is identical to sub, the same for the V bit applies (see sub for details)

		if(updateCarry)
		{
			m_asm.shr(d, asmjit::Imm(8));
			m_asm.shr(_v, asmjit::Imm(8));			
		}

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_lsl(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.shl(d.get().r32(), _shiftAmount + 8);	// + 8 to use native carry flag
		ccr_update_ifCarry(CCRB_C);
		m_asm.shr(d.get().r32(), 8);				// revert shift by 8
		ccr_update_ifZero(CCRB_Z);
		m_asm.bt(d.get().r32(), asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, d.get().r32());
	}

	inline void JitOps::alu_lsr(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.shr(d.get().r32(), _shiftAmount);
		ccr_update_ifCarry(CCRB_C);
		m_asm.cmp(d.get().r32(), asmjit::Imm(0));
		ccr_update_ifZero(CCRB_Z);
		m_asm.bt(d.get().r32(), asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, d.get().r32());
	}

	void JitOps::alu_mpy(TWord ab, RegGP& _s1, RegGP& _s2, bool _negate, bool _accumulate, bool _s1Unsigned, bool _s2Unsigned, bool _round)
	{
	//	assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

		if(!_s1Unsigned)
			signextend24to64(_s1.get());

		if(!_s2Unsigned)
			signextend24to64(_s2.get());

		m_asm.imul(_s1.get(), _s2.get());

		// fractional multiplication requires one post-shift to be correct
		m_asm.shl(_s1, asmjit::Imm(1));

		if(_negate)
			m_asm.neg(_s1);

		AluRef d(m_block, ab, _accumulate, true);

		if( _accumulate )
		{
			signextend56to64(d);
			m_asm.add(d, _s1.get());
		}
		else
		{
			m_asm.mov(d, _s1.get());
		}

		_s1.release();
		_s2.release();

		// Update SR

		if(!_round)
		{
			bool canOverflow = !_s1Unsigned || !_s2Unsigned;

			const auto vBit = canOverflow ? CCR_V : 0;

			ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z | vBit));

			if(canOverflow)
				m_dspRegs.mask56(d);
		}
		else
		{
			alu_rnd(ab, d);			
		}
	}
	
	inline void JitOps::alu_multiply(TWord op)
	{
		const auto round = op & 0x1;
		const auto mulAcc = (op>>1) & 0x1;
		const auto negative = (op>>2) & 0x1;
		const auto ab = (op>>3) & 0x1;
		const auto qqq = (op>>4) & 0x7;

		{
			RegGP s1(m_block);
			RegGP s2(m_block);

			decode_QQQQ_read(s1, s2, qqq);

			alu_mpy(ab, s1, s2, negative, mulAcc, false, false, round);
		}
	}

	inline void JitOps::alu_or(TWord ab, const JitRegGP& _v)
	{
		RegGP r(m_block);
		m_dspRegs.getALU(r, ab);

		m_asm.shl(_v, asmjit::Imm(24));

		m_asm.or_(r, _v);

		m_dspRegs.setALU(ab, r);

		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(r);

		m_asm.shr(r, asmjit::Imm(24));
		m_asm.and_(r, asmjit::Imm(0xffffff));
		ccr_update_ifZero(CCRB_Z);
		
		ccr_clear(CCR_V);
	}

	inline void JitOps::alu_rnd(TWord ab)
	{
		AluRef d(m_block, ab);
		alu_rnd(ab, d.get());
	}

	inline void JitOps::alu_rnd(TWord ab, const JitReg64& d)
	{
		RegGP rounder(m_block);
		m_asm.mov(rounder, asmjit::Imm(0x800000));

		{
			const ShiftReg shifter(m_block);
			m_asm.xor_(shifter, shifter.get());
			sr_getBitValue(shifter, SRB_S1);
			m_asm.shr(rounder, shifter.get());
			sr_getBitValue(shifter, SRB_S0);
			m_asm.shl(rounder, shifter.get());
		}

		signextend56to64(d);
		m_asm.add(d, rounder.get());

		m_asm.shl(rounder, asmjit::Imm(1));

		{
			// mask = all the bits to the right of, and including the rounding position
			const RegGP mask(m_block);
			m_asm.mov(mask, rounder.get());
			m_asm.dec(mask);

			const auto skipNoScalingMode = m_asm.newLabel();

			// if (!sr_test_noCache(SR_RM))
			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_SM));
			m_asm.jc(skipNoScalingMode);

			// convergent rounding. If all mask bits are cleared

			// then the bit to the left of the rounding position is cleared in the result
			// if (!(_alu.var & mask)) 
			//	_alu.var&=~(rounder<<1);
			m_asm.not_(rounder);

			{
				const RegGP aluIfAndWithMaskIsZero(m_block);
				m_asm.mov(aluIfAndWithMaskIsZero, d);
				m_asm.and_(aluIfAndWithMaskIsZero, rounder.get());

				rounder.release();

				{
					const auto temp = regReturnVal;
					m_asm.mov(temp, d);
					m_asm.and_(temp, mask.get());
					m_asm.cmovz(d, aluIfAndWithMaskIsZero.get());
				}
			}

			m_asm.bind(skipNoScalingMode);

			// all bits to the right of and including the rounding position are cleared.
			// _alu.var&=~mask;
			m_asm.not_(mask);
			m_asm.and_(d, mask.get());
		}

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z | CCR_V));
		m_dspRegs.mask56(d);
	}

	template<Instruction Inst> void JitOps::bitmod_ea(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord))
	{
		const auto area = getFieldValueMemArea<Inst>(op);

		auto eaType = effectiveAddressType<Inst>(op);

		// not sure if this can happen, iirc I've seen this once. Handle it
		if(eaType == Immediate)
			eaType = m_opWordB >= XIO_Reserved_High_First ? Peripherals : Memory;

		const RegGP regMem(m_block);

		switch (eaType)
		{
		case Peripherals:
			{
				const TWord offset = getOpWordB();
				m_block.mem().readPeriph(regMem, area, offset);
				(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
				m_block.mem().writePeriph(area, offset, regMem);
			}
			break;
		case Memory:
			{
				const TWord offset = getOpWordB();
				m_block.mem().readDspMemory(regMem, area, offset);
				(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
				m_block.mem().writeDspMemory(area, offset, regMem);
			}
			break;
		case Dynamic:
			{
				const RegGP offset(m_block);
				effectiveAddress<Inst>(offset, op);			
				readMemOrPeriph(regMem, area, offset);
				(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
				writeMemOrPeriph(area, offset, regMem);
			}
			break;
		}
	}
	
	template<Instruction Inst> void JitOps::bitmod_aa(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord))
	{
		const auto area = getFieldValueMemArea<Inst>(op);
		const auto addr = getFieldValue<Inst, Field_aaaaaa>(op);
		const RegGP regMem(m_block);
		m_block.mem().readDspMemory(regMem, area, addr);
		(this->*_bitmodFunc)(regMem, getBit<Inst>(op));
		m_block.mem().writeDspMemory(area, addr, regMem);
	}

	template<Instruction Inst> void JitOps::bitmod_ppqq(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord))
	{
		const RegGP r(m_block);
		readMem<Inst>(r, op);
		(this->*_bitmodFunc)(r, getBit<Inst>(op));
		writeMem<Inst>(op, r);
	}

	template<Instruction Inst> void JitOps::bitmod_D(TWord op, void( JitOps::*_bitmodFunc)(const JitReg64&, TWord))
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
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::op_Btst_aa(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_aa>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_aa>(op)));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::op_Btst_pp(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_pp>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_pp>(op)));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::op_Btst_qq(TWord op)
	{
		RegGP r(m_block);
		readMem<Btst_qq>(r, op);
		m_asm.bt(r.get().r32(), asmjit::Imm(getBit<Btst_qq>(op)));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::op_Btst_D(TWord op)
	{
		const auto dddddd	= getFieldValue<Btst_D,Field_DDDDDD>(op);
		const auto bit		= getBit<Btst_D>(op);

		const RegGP r(m_block);
		decode_dddddd_read(r.get().r32(), dddddd);

		m_asm.bt(r.get().r32(), asmjit::Imm(bit));
		ccr_update_ifCarry(CCRB_C);
	}

	inline void JitOps::op_Clr(TWord op)
	{
		const auto D = getFieldValue<Clr, Field_d>(op);
		m_dspRegs.clrALU(D);
		ccr_clear( static_cast<CCRMask>(CCR_E | CCR_N | CCR_V) );
		ccr_set( static_cast<CCRMask>(CCR_U | CCR_Z) );
	}

	inline void JitOps::op_Cmp_S1S2(TWord op)
	{
		const auto D = getFieldValue<Cmp_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmp_S1S2, Field_JJJ>(op);
		const RegGP regJ(m_block);
		decode_JJJ_read_56(regJ, JJJ, !D);
		alu_cmp(D, regJ, false);
	}

	inline void JitOps::op_Cmp_xxS2(TWord op)
	{
		const auto D = getFieldValue<Cmp_xxS2, Field_d>(op);
		const auto iiiiii = getFieldValue<Cmp_xxS2,Field_iiiiii>(op);
		
		TReg56 r56;
		convert( r56, TReg24(iiiiii) );

		const RegGP v(m_block);
		m_asm.mov(v, asmjit::Imm(r56.var));
		alu_cmp( D, v, false);
	}

	inline void JitOps::op_Cmp_xxxxS2(TWord op)
	{
		const auto D = getFieldValue<Cmp_xxxxS2, Field_d>(op);

		const TReg24 s( signextend<int,24>( getOpWordB() ) );

		TReg56 r56;
		convert( r56, s );

		const RegGP v(m_block);
		m_asm.mov(v, asmjit::Imm(r56.var));

		alu_cmp( D, v, false );
	}

	inline void JitOps::op_Cmpm_S1S2(TWord op)
	{
		const auto D = getFieldValue<Cmpm_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmpm_S1S2, Field_JJJ>(op);
		const RegGP r(m_block);
		decode_JJJ_read_56(r, JJJ, !D);
		alu_cmp(D, r, true);
	}

	inline void JitOps::op_Dec(TWord op)
	{
		const auto ab = getFieldValue<Dec,Field_d>(op);
		AluRef r(m_block, ab);

		m_asm.shl(r, asmjit::Imm(8));	// shift left by 8 bits to enable using the host carry bit
		m_asm.sub(r, asmjit::Imm(0x100));

		ccr_update_ifCarry(CCRB_C);

		m_asm.shr(r, asmjit::Imm(8));
		ccr_clear(CCR_V);				// never set in the simulator, even when wrapping around. Carry is set instead

		ccr_dirty(ab, r, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Div(TWord op)
	{
		const auto ab	= getFieldValue<Div,Field_d>(op);
		const auto jj	= getFieldValue<Div,Field_JJ>(op);

		AluRef d(m_block, ab);

		if(m_repMode == RepNone || m_repMode == RepLast)
		{
			// V and L updates
			// V: Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
			// L: Set if the Overflow bit (V) is set.
			// What we do is we check if bits 55 and 54 of the ALU are not identical (host parity bit cleared) and set V accordingly.
			// Nearly identical for L but L is only set, not cleared as it is sticky
			const RegGP r(m_block);
			m_asm.mov(r, d.get());
			m_asm.shr(r, asmjit::Imm(54));
			m_asm.and_(r, asmjit::Imm(0x3));
			m_asm.setnp(r.get().r8());
			ccr_update(r, CCRB_V);
			m_asm.shl(r, asmjit::Imm(CCRB_L));
			m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), r.get());
			m_ccrDirty = static_cast<CCRMask>(m_ccrDirty & ~(CCR_L | CCR_V));
		}

		{
			const RegGP s(m_block);
			decode_JJ_read(s, jj);

			m_asm.shl(s, asmjit::Imm(40));
			m_asm.sar(s, asmjit::Imm(16));

			const RegGP addOrSub(m_block);
			m_asm.mov(addOrSub, s.get());
			m_asm.xor_(addOrSub, d.get());

			m_asm.shl(d, asmjit::Imm(1));

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_C));
			m_asm.adc(d.get().r8(), asmjit::Imm(0));

			const auto dLsWord = regReturnVal;
			m_asm.mov(dLsWord, d.get());
			m_asm.and_(dLsWord, asmjit::Imm(0xffffff));

			const asmjit::Label sub = m_asm.newLabel();
			const asmjit::Label end = m_asm.newLabel();		

			m_asm.bt(addOrSub, asmjit::Imm(55));

			m_asm.jnc(sub);
			m_asm.add(d, s.get());
			m_asm.jmp(end);

			m_asm.bind(sub);
			m_asm.sub(d, s.get());

			m_asm.bind(end);
			m_asm.and_(d, asmjit::Imm(0xffffffffff000000));
			m_asm.or_(d, dLsWord);
		}

		// C is set if bit 55 of the result is cleared
		m_asm.bt(d, asmjit::Imm(55));
		ccr_update_ifNotCarry(CCRB_C);

		m_dspRegs.mask56(d);
	}

	inline void JitOps::op_Rep_Div(const TWord _op, const TWord _iterationCount)
	{
		m_block.getEncodedInstructionCount() += _iterationCount;

		const auto ab = getFieldValue<Div,Field_d>(_op);
		const auto jj = getFieldValue<Div,Field_JJ>(_op);

		AluRef d(m_block, ab);

		auto ccrUpdateVL = [&]()
		{
			// V and L updates
			// V: Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
			// L: Set if the Overflow bit (V) is set.
			// What we do is we check if bits 55 and 54 of the ALU are not identical (host parity bit cleared) and set V accordingly.
			// Nearly identical for L but L is only set, not cleared as it is sticky
			const RegGP r(m_block);
			m_asm.mov(r, d.get());
			m_asm.shr(r, asmjit::Imm(54));
			m_asm.and_(r, asmjit::Imm(0x3));
			m_asm.setnp(r.get().r8());
			ccr_update(r, CCRB_V);
			m_asm.shl(r, asmjit::Imm(CCRB_L));
			m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), r.get());
			m_ccrDirty = static_cast<CCRMask>(m_ccrDirty & ~(CCR_L | CCR_V));			
		};
		
		const auto finished = m_asm.newLabel();
		const auto regular = m_asm.newLabel();
		const RegGP s(m_block);
		decode_JJ_read(s, jj);
		/* TODO: broken
		if (_iterationCount<24)
		{
			{
				const RegGP t(m_block);
				m_asm.mov(t, s.get());
				m_asm.dec(t);
				m_asm.and_(t, s.get());
				m_asm.jnz(regular);
			}
			{
				const ShiftReg t(m_block);
				m_asm.bsr(t, s.get());
				m_asm.add(t, asmjit::Imm(_iterationCount + 1));
				m_asm.shr(d, t.get());
				m_asm.and_(d, asmjit::Imm((1<<_iterationCount)-1));
				m_asm.jmp(finished);
			}
		}
		*/
		m_asm.bind(regular);
		RegGP addOrSub(m_block);
		RegGP lc(m_block);
		RegGP carry(m_block);

		const auto loopIteration = [&](bool last)
		{
			m_asm.mov(addOrSub, s.get());
			m_asm.xor_(addOrSub, d.get());

			m_asm.shl(d, asmjit::Imm(1));

			m_asm.add(d.get().r8(), carry.get().r8());

			const auto dLsWord = regReturnVal;
			m_asm.mov(dLsWord, d.get());
			m_asm.and_(dLsWord, asmjit::Imm(0xffffff));

			const asmjit::Label sub = m_asm.newLabel();
			const asmjit::Label end = m_asm.newLabel();

			m_asm.bt(addOrSub, asmjit::Imm(55));

			m_asm.jnc(sub);
			m_asm.add(d, s.get());
			m_asm.jmp(end);

			m_asm.bind(sub);
			m_asm.sub(d, s.get());

			m_asm.bind(end);
			m_asm.and_(d, asmjit::Imm(0xffffffffff000000));
			m_asm.or_(d, dLsWord);

			// C is set if bit 55 of the result is cleared
			m_asm.bt(d, asmjit::Imm(55));
			if(last)
				ccr_update_ifNotCarry(CCRB_C);
			else
				m_asm.setnc(carry);
		};

		// once
		m_asm.shl(s, asmjit::Imm(40));
		m_asm.sar(s, asmjit::Imm(16));

		m_asm.mov(lc, _iterationCount-2);

		m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_C));
		m_asm.setc(carry);

		// loop
		const auto start = m_asm.newLabel();
		m_asm.bind(start);
		loopIteration(false);
		m_asm.dec(lc);
		m_asm.jnz(start);
		lc.release();

		// once
		ccrUpdateVL();
		loopIteration(true);

		m_dspRegs.mask56(d);
		m_asm.bind(finished);
	}

	inline void JitOps::op_Dmac(TWord op)
	{
		const auto ss			= getFieldValue<Dmac,Field_S, Field_s>(op);
		const bool ab			= getFieldValue<Dmac,Field_d>(op);
		const bool negate		= getFieldValue<Dmac,Field_k>(op);

		const auto qqqq			= getFieldValue<Dmac,Field_QQQQ>(op);

		const auto s1Unsigned	= ss > 1;
		const auto s2Unsigned	= ss > 0;

		RegGP s1(m_block);

		const RegGP s2(m_block);

		decode_QQQQ_read( s1, s2.get(), qqqq );

		if(s1Unsigned)	signextend24to64(s1);
		if(s2Unsigned)	signextend24to64(s2);

		m_asm.imul(s1, s2.get());

		// fractional multiplication requires one post-shift to be correct
		m_asm.sal(s1, asmjit::Imm(1));

		if( negate )
			m_asm.neg(s1);

		AluRef d(m_block, ab);

		signextend56to64(d);
		m_asm.sar(d, asmjit::Imm(24));

		m_asm.add(d, s1.get());
		s1.release();

		const auto& dOld = s2;
		m_asm.mov(dOld, d.get());

		m_dspRegs.mask56(d);

		// Update SR
		// detect overflow by sign-extending the actual result and comparing VS the non-sign-extended one. We've got overflow if they are different
		m_asm.cmp(dOld, d.get());

		ccr_update_ifNotZero(CCRB_V);
		ccr_l_update_by_v();

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Extractu_S1S2(TWord op)
	{
		const auto sss		= getFieldValue<Extractu_S1S2, Field_SSS>(op);
		const bool abDst	= getFieldValue<Extractu_S1S2, Field_D>(op);
		const bool abSrc	= getFieldValue<Extractu_S1S2, Field_s>(op);

		ccr_clear(CCR_C);
		ccr_clear(CCR_V);

		const RegGP widthOffset(m_block);
		decode_sss_read(widthOffset, sss);

		const ShiftReg width(m_block);
		m_asm.mov(width, widthOffset.get());
		m_asm.shr(width, asmjit::Imm(12));
		m_asm.and_(width, asmjit::Imm(0x3f));

		RegGP offset(m_block);
		m_asm.mov(offset, widthOffset.get());
		m_asm.and_(offset, asmjit::Imm(0x3f));

		m_asm.neg(width);
		m_asm.add(width, asmjit::Imm(56));

		const auto& mask = widthOffset;
		m_asm.mov(mask, asmjit::Imm(g_alu_max_56_u));
		m_asm.shr(mask, width.get());

		AluReg s(m_block, abSrc, abSrc != abDst);

		if(asmjit::CpuInfo::host().hasFeature(asmjit::x86::Features::kBMI2))
		{
			m_asm.shrx(s, s, offset.get());	
		}
		else
		{
			const ShiftReg shift(m_block);
			m_asm.mov(shift, offset.get());
			m_asm.shr(s, shift.get());
		}

		m_asm.and_(s, mask.get());

		offset.release();

		JitReg64 aluD;
		
		if(abSrc != abDst)
		{
			AluRef d(m_block, abDst, false, true);
			m_asm.mov(d, s.get());
			aluD = d.get();
		}
		else
		{
			aluD = s.get();			
		}

		ccr_dirty(abDst, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Extractu_CoS2(TWord op)
	{
		const bool abDst = getFieldValue<Extractu_CoS2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_CoS2, Field_s>(op);

		const auto widthOffset = getOpWordB();
		const auto width = (widthOffset >> 12) & 0x3f;
		const auto offset = widthOffset & 0x3f;

		const auto mask = g_alu_max_56_u >> (56 - width);

		AluReg d(m_block, abDst, false, abSrc != abDst);

		if(abSrc != abDst)
			m_dspRegs.getALU(d, abSrc);

		m_asm.shr(d, asmjit::Imm(offset));
		m_asm.and_(d, asmjit::Imm(mask));

		ccr_clear(CCR_C);
		ccr_clear(CCR_V);
		ccr_dirty(abDst, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Inc(TWord op)
	{
		const auto ab = getFieldValue<Dec,Field_d>(op);
		AluRef r(m_block, ab);

		m_asm.shl(r, asmjit::Imm(8));		// shift left by 8 bits to enable using the host carry bit
		m_asm.add(r, asmjit::Imm(0x100));

		ccr_update_ifCarry(CCRB_C);

		m_asm.shr(r, asmjit::Imm(8));

		ccr_clear(CCR_V);					// never set in the simulator, even when wrapping around. Carry is set instead

		ccr_dirty(ab, r, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Lra_xxxx(TWord op)
	{
		const auto ddddd = getFieldValue<Lra_xxxx, Field_ddddd>(op);
		const auto ea = m_pcCurrentOp + pcRelativeAddressExt<Lra_xxxx>();

		const RegGP r(m_block);
		m_asm.mov(r.get().r32(), asmjit::Imm(ea));
		decode_dddddd_write(ddddd, r.get().r32());
	}

	inline void JitOps::op_Lsl_D(TWord op)
	{
		const auto D = getFieldValue<Lsl_D,Field_D>(op);
		alu_lsl(D, 1);
	}

	inline void JitOps::op_Lsl_ii(TWord op)
	{
		const auto shiftAmount = getFieldValue<Lsl_ii,Field_iiiii>(op);
		const auto D = getFieldValue<Lsl_ii,Field_D>(op);

		alu_lsl(D, shiftAmount);
	}

	inline void JitOps::op_Lsr_D(TWord op)
	{
		const auto D = getFieldValue<Lsr_D,Field_D>(op);
		alu_lsr(D, 1);
	}

	inline void JitOps::op_Lsr_ii(TWord op)
	{
		const auto shiftAmount = getFieldValue<Lsr_ii,Field_iiiii>(op);
		const auto abDst = getFieldValue<Lsr_ii,Field_D>(op);

		alu_lsr(abDst, shiftAmount);
	}

	template<Instruction Inst, bool Accumulate, bool Round> void JitOps::op_Mac_S(TWord op)
	{
		const auto sssss	= getFieldValue<Inst,Field_sssss>(op);
		const auto qq		= getFieldValue<Inst,Field_QQ>(op);
		const auto	ab		= getFieldValue<Inst,Field_d>(op);
		const auto	negate	= getFieldValue<Inst,Field_k>(op);

		RegGP s1(m_block);
		decode_QQ_read(s1, qq);

		RegGP s2(m_block);
		m_asm.mov(s2, asmjit::Imm(DSP::decode_sssss(sssss)));

		alu_mpy(ab, s1, s2, negate, Accumulate, false, false, Round);
	}

	template<Instruction Inst, bool Accumulate> void JitOps::op_Mpy_su(TWord op)
	{
		const bool ab		= getFieldValue<Inst,Field_d>(op);
		const bool negate	= getFieldValue<Inst,Field_k>(op);
		const bool uu		= getFieldValue<Inst,Field_s>(op);
		const TWord qqqq	= getFieldValue<Inst,Field_QQQQ>(op);

		RegGP s1(m_block);
		RegGP s2(m_block);
		decode_QQQQ_read( s1, s2, qqqq );

		alu_mpy(ab, s1, s2, negate, Accumulate, !uu, true, false);
	}

	inline void JitOps::op_Mpyi(TWord op)
	{
		const bool	ab		= getFieldValue<Mpyi,Field_d>(op);
		const bool	negate	= getFieldValue<Mpyi,Field_k>(op);
		const TWord qq		= getFieldValue<Mpyi,Field_qq>(op);

		RegGP s(m_block);
		getOpWordB(s);

		RegGP reg(m_block);
		decode_qq_read(reg, qq);

		alu_mpy( ab, reg, s, negate, false, false, false, false);
	}

	inline void JitOps::op_Neg(TWord op)
	{
		const auto D = getFieldValue<Neg, Field_d>(op);

		AluRef r(m_block, D);

		signextend56to64(r);
		m_asm.neg(r);
		ccr_update_ifLess(CCRB_N);

		m_dspRegs.mask56(r);
		
		ccr_clear(CCR_V);

		ccr_dirty(D, r, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::op_Nop(TWord op)
	{
	}

	inline void JitOps::op_Not(TWord op)
	{
		const auto ab = getFieldValue<Not, Field_d>(op);

		{
			const RegGP d(m_block);
			getALU1(d, ab);
			m_asm.not_(d.get().r32());
			m_asm.and_(d, asmjit::Imm(0xffffff));
			setALU1(ab, d.get().r32());

			m_asm.bt(d, asmjit::Imm(23));
			ccr_update_ifCarry(CCRB_N);					// Set if bit 47 of the result is set

			m_asm.cmp(d, asmjit::Imm(0));
			ccr_update_ifZero(CCRB_Z);					// Set if bits 47–24 of the result are 0
		}

		ccr_clear(CCR_V);								// Always cleared
	}

	inline void JitOps::op_Or_SD(TWord op)
	{
		const auto D = getFieldValue<Or_SD, Field_d>(op);
		const auto JJ = getFieldValue<Or_SD, Field_JJ>(op);
		RegGP r(m_block);
		decode_JJ_read(r, JJ);
		alu_or(D, r.get());
	}

	inline void JitOps::op_Ori(TWord op)
	{
		const auto ee		= getFieldValue<Ori,Field_EE>(op);
		const auto iiiiii	= getFieldValue<Ori,Field_iiiiiiii>(op);

		RegGP r(m_block);
		decode_EE_read(r, ee);
		m_asm.or_(r, asmjit::Imm(iiiiii));
		decode_EE_write(r, ee);
	}

	inline void JitOps::op_Rnd(TWord op)
	{
		const auto d = getFieldValue<Rnd, Field_d>(op);
		alu_rnd(d);		
	}

	inline void JitOps::op_Rol(TWord op)
	{
		const auto D = getFieldValue<Rol, Field_d>(op);

		RegGP r(m_block);
		getALU1(r, D);

		const RegGP prevCarry(m_block);
		m_asm.xor_(prevCarry, prevCarry.get());

		ccr_getBitValue(prevCarry, CCRB_C);

		m_asm.bt(r, asmjit::Imm(23));						// Set if bit 47 of the destination operand is set, and cleared otherwise
		ccr_update_ifCarry(CCRB_C);

		m_asm.shl(r.get(), asmjit::Imm(1));
		ccr_n_update_by23(r.get());							// Set if bit 47 of the result is set

		m_asm.or_(r, prevCarry.get());						// Set if bits 47–24 of the result are 0
		ccr_update_ifZero(CCRB_Z);
		setALU1(D, r.get().r32());

		ccr_clear(CCR_V);									// This bit is always cleared
	}

	inline void JitOps::op_Sub_SD(TWord op)
	{
		const auto D = getFieldValue<Sub_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Sub_SD, Field_JJJ>(op);

		RegGP v(m_block);
		decode_JJJ_read_56(v, JJJ, !D);
		alu_sub(D, v);
	}

	inline void JitOps::op_Sub_xx(TWord op)
	{
		const auto ab		= getFieldValue<Sub_xx,Field_d>(op);
		const TWord iiiiii	= getFieldValue<Sub_xx,Field_iiiiii>(op);

		alu_sub( ab, asmjit::Imm(iiiiii) );
	}

	inline void JitOps::op_Sub_xxxx(TWord op)
	{
		const auto ab = getFieldValue<Sub_xxxx,Field_d>(op);

		RegGP r(m_block);
		getOpWordB(r.get());
		signed24To56(r);
		alu_sub( ab, r );		// TODO use immediate data
	}

	inline void JitOps::op_Tcc_S1D1(TWord op)
	{
		const auto JJJ = getFieldValue<Tcc_S1D1,Field_JJJ>(op);
		const bool ab = getFieldValue<Tcc_S1D1,Field_d>(op);

		If(m_block, [&](auto _toFalse)
		{
			checkCondition<Tcc_S1D1>(op);
			m_asm.jz(_toFalse);
		}, [&]()
		{
			const RegGP r(m_block);
			decode_JJJ_read_56(r, JJJ, !ab);
			m_dspRegs.setALU(ab, r);
		});
	}

	inline void JitOps::op_Tcc_S1D1S2D2(TWord op)
	{
		const auto TTT		= getFieldValue<Tcc_S1D1S2D2,Field_TTT>(op);
		const auto JJJ		= getFieldValue<Tcc_S1D1S2D2,Field_JJJ>(op);
		const auto ttt		= getFieldValue<Tcc_S1D1S2D2,Field_ttt>(op);
		const auto ab		= getFieldValue<Tcc_S1D1S2D2,Field_d>(op);

		If(m_block, [&](auto _toFalse)
		{
			checkCondition<Tcc_S1D1S2D2>(op);
			m_asm.jz(_toFalse);
		}, [&]()
		{
			{
				const RegGP r(m_block);
				decode_JJJ_read_56(r, JJJ, !ab);
				m_dspRegs.setALU(ab, r);
			}

			if(TTT != ttt)
			{
				AguRegR r(m_block, ttt, true);
				m_dspRegs.setR(TTT, r);
			}
		});
	}

	inline void JitOps::op_Tcc_S2D2(TWord op)
	{
		const auto TTT	= getFieldValue<Tcc_S2D2,Field_TTT>(op);
		const auto ttt	= getFieldValue<Tcc_S2D2,Field_ttt>(op);

		if(TTT == ttt)
			return;

		If(m_block, [&](auto _toFalse)
		{
			checkCondition<Tcc_S2D2>(op);
			m_asm.jz(_toFalse);
		}, [&]()
		{
			AguRegR r(m_block, ttt, true);
			m_dspRegs.setR(TTT, r);
		});
	}

	inline void JitOps::op_Tfr(TWord op)
	{
		const auto D = getFieldValue<Tfr, Field_d>(op);
		const auto JJJ = getFieldValue<Tfr, Field_JJJ>(op);

		const RegGP r(m_block);

		decode_JJJ_read_56(r, JJJ, !D);
		m_dspRegs.setALU(D, r);
	}

	inline void JitOps::op_Tst(TWord op)
	{
		const auto D = getFieldValue<Tst, Field_d>(op);
		const RegGP zero(m_block);
		m_asm.xor_(zero, zero.get());
		alu_cmp(D, zero, false, false);
	}
}
