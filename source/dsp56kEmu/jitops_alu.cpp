#include "jitops.h"

#include "jitops_alu.inl"

#include <cmath>	// log2

namespace dsp56k
{
	constexpr int64_t g_alu_max_56 = 0x7FFFFFFFFFFFFF;
	constexpr int64_t g_alu_min_56 = -0x80000000000000;
	constexpr uint64_t g_alu_max_56_u = 0xffffffffffffff;

	void JitOps::XYto56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		signextend48to56(_dst);
	}

	void JitOps::op_Abs(TWord op)
	{
		const auto ab = getFieldValue<Abs, Field_d>(op);

		AluRef ra(m_block, ab);							// Load ALU

		m_asm.shl(ra, asmjit::Imm(8));				// extend to 64 bits

		alu_abs(ra);

		m_asm.shr(ra, asmjit::Imm(8));

	//	sr_v_update(d);
	//	sr_l_update_by_v();
		ccr_dirty(ab, ra, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_add(const TWord _ab, const JitReg64& _v)
	{
		AluRef alu(m_block, _ab);

		m_asm.add(alu.get(), _v);
		
		if(!m_disableCCRUpdates)
		{
			CcrBatchUpdate bu(*this, CCR_C, CCR_V);

			copyBitToCCR(alu, 56, CCRB_C);

//			ccr_clear(CCR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops
		}

		m_dspRegs.mask56(alu);

		if(!m_disableCCRUpdates)
			ccr_dirty(_ab, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_add(const TWord _ab, const uint8_t _v)
	{
		RegGP r(m_block);
		unsignedImmediateToAlu(r, _v);
		alu_add(_ab, r);
	}

	void JitOps::alu_sub(const TWord _ab, const JitReg64& _v)
	{
		AluRef alu(m_block, _ab);

		m_asm.sub(alu, _v);

		if(!m_disableCCRUpdates)
		{
			CcrBatchUpdate bu(*this, CCR_C, CCR_V);

			copyBitToCCR(alu, 56, CCRB_C);

//			ccr_clear(CCR_V); batch cleared
		}

		m_dspRegs.mask56(alu);

		if(!m_disableCCRUpdates)
			ccr_dirty(_ab, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_sub(const TWord _ab, const uint8_t _v)
	{
		RegGP r(m_block);
		unsignedImmediateToAlu(r, _v);
		alu_sub(_ab, r);
	}

	void JitOps::unsignedImmediateToAlu(const JitReg64& _r, const uint8_t _i) const
	{
		m_asm.mov(_r, asmjit::Imm(static_cast<uint32_t>(_i) << 24));
	}

	void JitOps::op_Add_SD(TWord op)
	{
		const auto D = getFieldValue<Add_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Add_SD, Field_JJJ>(op);

		const auto v = decode_JJJ_read_56(JJJ, !D);
		alu_add(D, r64(v.get()));
	}

	void JitOps::op_Add_xx(TWord op)
	{
		const auto iiiiii = getFieldValue<Add_xx, Field_iiiiii>(op);
		const auto ab = getFieldValue<Add_xx, Field_d>(op);

		alu_add(ab, static_cast<uint8_t>(iiiiii));
	}

	void JitOps::op_Add_xxxx(TWord op)
	{
		const auto ab = getFieldValue<Add_xxxx, Field_d>(op);

		const auto opB = signed24To56(getOpWordB());

		RegGP r(m_block);
		m_asm.mov(r64(r), asmjit::Imm(opB));

		alu_add(ab, r);
	}

	void JitOps::op_Addl(TWord op)
	{
		// D = 2 * D + S

		const auto ab = getFieldValue<Addl, Field_d>(op);

		AluReg aluD(m_block, ab);

		signextend56to64(aluD);

#ifdef HAVE_ARM64
		m_asm.lsl(aluD, aluD, asmjit::Imm(1));
#else
		m_asm.sal(aluD, asmjit::Imm(1));
#endif
		{
			AluReg aluS(m_block, ab ? 0 : 1, true);

			signextend56to64(aluS);

#ifdef HAVE_ARM64
			m_asm.adds(aluD, aluD, aluS.get());
#else
			m_asm.add(aluD, aluS.get());
#endif
		}

		ccr_update_ifCarry(CCRB_C);

		m_dspRegs.mask56(aluD);

		ccr_clear(CCR_V);	// TODO: Set if overflow has occurred in the A or B result or the MSB of the destination operand is changed as a result of the instruction’s left shift.
		ccr_dirty(ab, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Addr(TWord op)
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
			const RegScratch aluMax(m_block);
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(aluD, aluMax);
		}

		ccr_update_ifGreater(CCRB_C);

		m_dspRegs.mask56(aluD);

		ccr_clear(CCR_V);			// TODO: Changed according to the standard definition.
		ccr_dirty(ab, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_And_SD(TWord op)
	{
		const auto D = getFieldValue<And_SD, Field_d>(op);
		const auto JJ = getFieldValue<And_SD, Field_JJ>(op);

		DspValue r(m_block);
		decode_JJ_read(r, JJ);
		alu_and(D, r);
	}

	void JitOps::op_And_xx(TWord op)
	{
		const auto ab = getFieldValue<And_xx, Field_d>(op);
		const auto xxxx = getFieldValue<And_xx, Field_iiiiii>(op);

		DspValue r(m_block, xxxx, DspValue::Immediate24);
		alu_and(ab, r);
	}

	void JitOps::op_And_xxxx(TWord op)
	{
		const auto ab = getFieldValue<And_xxxx, Field_d>(op);

		DspValue r(m_block);
		getOpWordB(r);
		alu_and(ab, r);
	}

	void JitOps::op_Andi(TWord op)
	{
		const auto ee = getFieldValue<Andi, Field_EE>(op);
		const auto iiiiii = getFieldValue<Andi, Field_iiiiiiii>(op);

		const bool ccr = ee == 1;

		if(ccr)
		{
			// clear all CCR bits that are not set in the immediate value
			for(uint32_t i=0; i<8; ++i)
			{
				const auto mask = (1 << i);

				if(!(iiiiii & mask))
				{
					ccr_clear(static_cast<CCRMask>(mask));
				}
			}

			return;
		}

		RegGP r(m_block);
		decode_EE_read(r, ee);
#ifdef HAVE_ARM64
		{
			RegGP i(m_block);
			m_asm.mov(i, asmjit::Imm(iiiiii));
			m_asm.and_(r, i.get());
		}
#else
		m_asm.and_(r, asmjit::Imm(iiiiii));
#endif
		decode_EE_write(r, ee);
	}

	void JitOps::op_Asl_D(TWord op)
	{
		const auto D = getFieldValue<Asl_D, Field_d>(op);

		alu_asl(D, D, nullptr, 1);
	}

	void JitOps::op_Asl_ii(TWord op)
	{
		const TWord shiftAmount = getFieldValue<Asl_ii, Field_iiiiii>(op);

		const bool abDst = getFieldValue<Asl_ii, Field_D>(op);
		const bool abSrc = getFieldValue<Asl_ii, Field_S>(op);

		alu_asl(abSrc, abDst, nullptr, shiftAmount);
	}

	void JitOps::op_Asl_S1S2D(TWord op)
	{
		const auto sss = getFieldValue<Asl_S1S2D, Field_sss>(op);
		const bool abDst = getFieldValue<Asl_S1S2D, Field_D>(op);
		const bool abSrc = getFieldValue<Asl_S1S2D, Field_S>(op);

		DspValue r(m_block);
		decode_sss_read(r, sss);
		m_asm.and_(r.get(), asmjit::Imm(0x3f));	// "In the control register S1: bits 5–0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		const ShiftReg shiftReg(m_block);
		m_asm.mov(r32(shiftReg), r32(r));
		r.release();
		alu_asl(abSrc, abDst, &shiftReg);
	}

	void JitOps::op_Asr_D(TWord op)
	{
		const auto D = getFieldValue<Asr_D, Field_d>(op);
		alu_asr(D, D, nullptr, 1);
	}

	void JitOps::op_Asr_ii(TWord op)
	{
		const TWord shiftAmount = getFieldValue<Asr_ii, Field_iiiiii>(op);

		const bool abDst = getFieldValue<Asr_ii, Field_D>(op);
		const bool abSrc = getFieldValue<Asr_ii, Field_S>(op);

		alu_asr(abSrc, abDst, nullptr, shiftAmount);
	}

	void JitOps::op_Asr_S1S2D(TWord op)
	{
		const auto sss = getFieldValue<Asr_S1S2D, Field_sss>(op);
		const auto abDst = getFieldValue<Asr_S1S2D, Field_D>(op);
		const auto abSrc = getFieldValue<Asr_S1S2D, Field_S>(op);

		DspValue r(m_block);
		decode_sss_read(r, sss);
		m_asm.and_(r.get(), asmjit::Imm(0x3f));	// "In the control register S1: bits 5–0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		const ShiftReg shifter(m_block);
		m_asm.mov(r32(shifter), r32(r));
		r.release();
		alu_asr(abSrc, abDst, &shifter);
	}

	void JitOps::alu_cmp(TWord ab, const JitReg64& _v, bool _magnitude)
	{
		AluReg d(m_block, ab, true);

		m_asm.sal(d.get(), asmjit::Imm(8));
		m_asm.sal(_v, asmjit::Imm(8));

		if (_magnitude)
		{
			alu_abs(d);
			alu_abs(_v);
		}

		// C and V are both cleared. Only C is updated as V is cleared always
		{
			CcrBatchUpdate u(*this, static_cast<CCRMask>(CCR_C | CCR_V));

#ifdef HAVE_ARM64
			m_asm.subs(d, d, _v);
			ccr_update_ifNotCarry(CCRB_C);		// we, THAT is unexpected: On ARM, carry means unsigned >= while it means unsigned < on 56k and intel
#else
			m_asm.sub(d, _v);
			ccr_update_ifCarry(CCRB_C);
#endif
		}

		m_asm.shr(d, asmjit::Imm(8));
		m_asm.shr(_v, asmjit::Imm(8));

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_eor(const TWord ab, DspValue& _v)
	{
		AluRef r(m_block, ab);

		// we cannot specify an arbitrary immediate shifted up 24 bits so we need to do convert it to a register first
		if(_v.isImm24())
			_v.toTemp();
#ifdef HAVE_ARM64
		m_asm.eor(r, r, r64(_v.get()), asmjit::arm::lsl(24));
#else
		m_asm.shl(r64(_v.get()), asmjit::Imm(24));
		m_asm.xor_(r, r64(_v.get()));
#endif
		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(r);

		const RegGP t(m_block);
#ifdef HAVE_ARM64
		m_asm.ubfx(r64(t), r64(r), asmjit::Imm(24), asmjit::Imm(24));
		m_asm.test_(t);
#else
		m_asm.ror(t.get(), r, 24);
		m_asm.test(t, asmjit::Imm(0xffffff));
#endif
		ccr_update_ifZero(CCRB_Z);

		ccr_clear(CCR_V);
	}

	void JitOps::alu_mpy(TWord ab, DspValue& _s1, DspValue& _s2, bool _negate, bool _accumulate, bool _s1Unsigned, bool _s2Unsigned, bool _round)
	{
//		assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

		AluRef d(m_block, ab, _accumulate, true);

#ifdef HAVE_ARM64
		if (_negate)
		{
			m_asm.smnegl(r64(_s1), r32(_s1), r32(_s2));
		}
		else
		{
			if(_s2.isImmediate() && asmjit::Support::isPowerOf2(_s2.imm24()))
			{
				const auto shift = static_cast<uint32_t>(log2(_s2.imm24()));
				m_asm.shl(r64(_s1), asmjit::Imm(shift));
			}
			else
			{
				m_asm.smull(r64(_s1), r32(_s1), r32(_s2));
			}
		}

		_s2.release();

		if (_accumulate)
		{
			signextend56to64(d);
			m_asm.add(d, d, r64(_s1), asmjit::arm::lsl(1));		// fractional multiplication requires one post-shift to be correct
		}
		else
		{
			m_asm.lsl(d, r64(_s1), asmjit::Imm(1));					// fractional multiplication requires one post-shift to be correct
		}
#else
		if(_s2.isImmediate())
		{
			const int64_t i = static_cast<int64_t>(_s2.imm24()) * 2;

			if(_negate)
			{
				m_asm.imul(r64(_s1), asmjit::Imm(-i));
			}
			else
			{
				if (asmjit::Support::isPowerOf2(i))
				{
					const auto shift = static_cast<uint32_t>(log2(i));
					m_asm.shl(r64(_s1), asmjit::Imm(shift));
				}
				else
				{
					m_asm.imul(r64(_s1), asmjit::Imm(i));
				}
			}
			if (_accumulate)
			{
				signextend56to64(d);
				m_asm.add(d, r64(_s1));
			}
			else
			{
				m_asm.mov(d, r64(_s1));
			}
		}
		else
		{
			m_asm.imul(r64(_s1), r64(_s2));
			_s2.release();

			if(!_accumulate && !_negate)
			{
				m_asm.lea(r64(d.get()), asmjit::x86::ptr(r64(_s1.get()), r64(_s1.get())));
			}
			else
			{
				if (_accumulate)
					signextend56to64(d);

				if(_accumulate && !_negate)
				{
					m_asm.lea(d, asmjit::x86::ptr(d, r64(_s1), 1));
				}
				else
				{
					// fractional multiplication requires one post-shift to be correct
					m_asm.add(r64(_s1), r64(_s1));	// add r,r is faster than shl r,1 on Haswell, can run on more ports and has a TP of 0.25 vs 0.5

					if(_accumulate && _negate)
					{
						m_asm.sub(d, r64(_s1));
					}
					else/* if(_negate)*/
					{
						m_asm.neg(r64(_s1));
						m_asm.mov(d, r64(_s1));
					}
				}
			}
		}
#endif

		_s1.release();

		// Update SR

		if (!_round)
		{
			const bool canOverflow = !_s1Unsigned || !_s2Unsigned;

			const auto vBit = canOverflow ? CCR_V : 0;

			ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z | vBit));

			if (canOverflow || _negate)
				m_dspRegs.mask56(d);
		}
		else
		{
			alu_rnd(ab, d, !_accumulate);
		}
	}

	void JitOps::alu_multiply(TWord op)
	{
		const auto round = op & 0x1;
		const auto mulAcc = (op >> 1) & 0x1;
		const auto negative = (op >> 2) & 0x1;
		const auto ab = (op >> 3) & 0x1;
		const auto qqq = (op >> 4) & 0x7;

		{
			DspValue s1(m_block);
			DspValue s2(m_block);

			decode_QQQQ_read(s1, true, s2, true, qqq);

			alu_mpy(ab, s1, s2, negative, mulAcc, false, false, round);
		}
	}

	void JitOps::alu_or(TWord ab, DspValue& _v)
	{
		AluRef r(m_block, ab);

		// we cannot specify an arbitrary immediate shifted up 24 bits so we need to do convert it to a register first
		if(_v.isImm24())
			_v.toTemp();

		m_asm.shl(r64(_v.get()), asmjit::Imm(24));
		m_asm.or_(r, r64(_v.get()));

		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(r);

		const RegGP t(m_block);
#ifdef HAVE_ARM64
		m_asm.ubfx(r64(t), r64(r), asmjit::Imm(24), asmjit::Imm(24));
		m_asm.test_(t);
#else
		m_asm.ror(t.get(), r, 24);
		m_asm.test(t, asmjit::Imm(0xffffff));
#endif
		ccr_update_ifZero(CCRB_Z);

		ccr_clear(CCR_V);
	}

	void JitOps::alu_rnd(TWord ab)
	{
		AluRef d(m_block, ab);
		alu_rnd(ab, d.get());
	}

	void JitOps::op_Bchg_ea(TWord op) { bitmod_ea<Bclr_ea>(op, &JitOps::alu_bchg); }
	void JitOps::op_Bchg_aa(TWord op) { bitmod_aa<Bclr_aa>(op, &JitOps::alu_bchg); }
	void JitOps::op_Bchg_pp(TWord op) { bitmod_ppqq<Bclr_pp>(op, &JitOps::alu_bchg); }
	void JitOps::op_Bchg_qq(TWord op) { bitmod_ppqq<Bclr_qq>(op, &JitOps::alu_bchg); }
	void JitOps::op_Bchg_D(TWord op) { bitmod_D<Bclr_D>(op, &JitOps::alu_bchg); }

	void JitOps::op_Bclr_ea(TWord op) { bitmod_ea<Bclr_ea>(op, &JitOps::alu_bclr); }
	void JitOps::op_Bclr_aa(TWord op) { bitmod_aa<Bclr_aa>(op, &JitOps::alu_bclr); }
	void JitOps::op_Bclr_pp(TWord op) { bitmod_ppqq<Bclr_pp>(op, &JitOps::alu_bclr); }
	void JitOps::op_Bclr_qq(TWord op) { bitmod_ppqq<Bclr_qq>(op, &JitOps::alu_bclr); }
	void JitOps::op_Bclr_D(TWord op) { bitmod_D<Bclr_D>(op, &JitOps::alu_bclr); }

	void JitOps::op_Bset_ea(TWord op) { bitmod_ea<Bset_ea>(op, &JitOps::alu_bset); }
	void JitOps::op_Bset_aa(TWord op) { bitmod_aa<Bset_aa>(op, &JitOps::alu_bset); }
	void JitOps::op_Bset_pp(TWord op) { bitmod_ppqq<Bset_pp>(op, &JitOps::alu_bset); }
	void JitOps::op_Bset_qq(TWord op) { bitmod_ppqq<Bset_qq>(op, &JitOps::alu_bset); }
	void JitOps::op_Bset_D(TWord op) { bitmod_D<Bset_D>(op, &JitOps::alu_bset); }

	void JitOps::op_Clr(TWord op)
	{
		const auto D = getFieldValue<Clr, Field_d>(op);
		m_dspRegs.clrALU(D);
#ifdef HAVE_ARM64
		ccr_clear(CCR_E);	// see ccr_clear why this workaround is needed for ARMv8
		ccr_clear(CCR_N);
		ccr_clear(CCR_V);
		ccr_set(CCR_U);
		ccr_set(CCR_Z);
#else
		ccr_clear(static_cast<CCRMask>(CCR_E | CCR_N | CCR_V));
		ccr_set(static_cast<CCRMask>(CCR_U | CCR_Z));
#endif
	}

	void JitOps::op_Cmp_S1S2(TWord op)
	{
		const auto D = getFieldValue<Cmp_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmp_S1S2, Field_JJJ>(op);
		const auto regJ = decode_JJJ_read_56(JJJ, !D);
		alu_cmp(D, r64(regJ.get()), false);
	}

	void JitOps::op_Cmp_xxS2(TWord op)
	{
		const auto D = getFieldValue<Cmp_xxS2, Field_d>(op);
		const auto iiiiii = getFieldValue<Cmp_xxS2, Field_iiiiii>(op);

		TReg56 r56;
		convert(r56, TReg24(iiiiii));

		const RegGP v(m_block);
		m_asm.mov(v, asmjit::Imm(r56.var));
		alu_cmp(D, v, false);
	}

	void JitOps::op_Cmp_xxxxS2(TWord op)
	{
		const auto D = getFieldValue<Cmp_xxxxS2, Field_d>(op);

		const TReg24 s(signextend<int, 24>(getOpWordB()));

		TReg56 r56;
		convert(r56, s);

		const RegGP v(m_block);
		m_asm.mov(v, asmjit::Imm(r56.var));

		alu_cmp(D, v, false);
	}

	void JitOps::op_Cmpm_S1S2(TWord op)
	{
		const auto D = getFieldValue<Cmpm_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmpm_S1S2, Field_JJJ>(op);
		const auto r = decode_JJJ_read_56(JJJ, !D);
		alu_cmp(D, r64(r.get()), true);
	}

	void JitOps::op_Dec(TWord op)
	{
		const auto ab = getFieldValue<Dec, Field_d>(op);
		AluRef r(m_block, ab);

		m_asm.shl(r, asmjit::Imm(8));	// shift left by 8 bits to enable using the host carry bit

#ifdef HAVE_ARM64
		m_asm.subs(r, r, asmjit::Imm(0x100));
		ccr_update_ifNotCarry(CCRB_C);
#else
		m_asm.sub(r, asmjit::Imm(0x100));
		ccr_update_ifCarry(CCRB_C);
#endif

		m_asm.shr(r, asmjit::Imm(8));
		ccr_clear(CCR_V);				// never set in the simulator, even when wrapping around. Carry is set instead

		ccr_dirty(ab, r, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Dmac(TWord op)
	{
		const auto ss = getFieldValue<Dmac, Field_S, Field_s>(op);
		const bool ab = getFieldValue<Dmac, Field_d>(op);
		const bool negate = getFieldValue<Dmac, Field_k>(op);

		const auto qqqq = getFieldValue<Dmac, Field_QQQQ>(op);

		const auto s1Unsigned = ss > 1;
		const auto s2Unsigned = ss > 0;

		DspValue s1(m_block);

		DspValue s2(m_block);

		decode_QQQQ_read(s1, !s1Unsigned, s2, !s2Unsigned, qqqq);

#ifdef HAVE_ARM64
		m_asm.smull(r64(s1), r32(s1), r32(s2));
#else
		m_asm.imul(r64(s1), r64(s2));
#endif
		// fractional multiplication requires one post-shift to be correct
		m_asm.sal(r64(s1), asmjit::Imm(1));

		if (negate)
			m_asm.neg(r64(s1));

		AluRef d(m_block, ab);

		signextend56to64(d);
		m_asm.sar(d, asmjit::Imm(24));

		m_asm.add(d, r64(s1));
		s1.release();

		const auto& dOld = r64(s2);
		m_asm.mov(dOld, d.get());

		m_dspRegs.mask56(d);

		// Update SR
		// detect overflow by sign-extending the actual result and comparing VS the non-sign-extended one. We've got overflow if they are different
		m_asm.cmp(dOld, d.get());

		ccr_update_ifNotZero(CCRB_V);
		ccr_l_update_by_v();

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Eor_SD(TWord op)
	{
		const auto D = getFieldValue<Or_SD, Field_d>(op);
		const auto JJ = getFieldValue<Or_SD, Field_JJ>(op);
		DspValue v(m_block);
		decode_JJ_read(v, JJ);
		alu_eor(D, v);
	}

	void JitOps::op_Eor_xx(TWord op)
	{
		errNotImplemented(op);
	}

	void JitOps::op_Extractu_S1S2(TWord op)
	{
		const auto sss = getFieldValue<Extractu_S1S2, Field_SSS>(op);
		const bool abDst = getFieldValue<Extractu_S1S2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_S1S2, Field_s>(op);

		ccr_clear(CCR_C);
		ccr_clear(CCR_V);

		DspValue widthOffset(m_block);
		decode_sss_read(widthOffset, sss);

		const ShiftReg width(m_block);
		m_asm.mov(r32(width), r32(widthOffset));
		m_asm.shr(r32(width), asmjit::Imm(12));
		m_asm.and_(r32(width), asmjit::Imm(0x3f));

		RegGP offset(m_block);
		m_asm.mov(r32(offset), r32(widthOffset));
		m_asm.and_(r32(offset), asmjit::Imm(0x3f));

		m_asm.neg(width);
		m_asm.add(width, asmjit::Imm(56));

		const auto& mask = r64(widthOffset);
		m_asm.mov(mask, asmjit::Imm(g_alu_max_56_u));
		m_asm.shr(mask, shiftOperand(width.get()));

		AluReg s(m_block, abSrc, abSrc != abDst);
#ifdef HAVE_X86_64
		if (m_asm.hasBMI2())
		{
			m_asm.shrx(s, s, offset.get());
		}
		else
#endif
		{
			const ShiftReg& shift = width;
			m_asm.mov(shift, offset.get());
			m_asm.shr(s, shiftOperand(shift.get()));
		}

		m_asm.and_(s, mask);

		offset.release();

		JitReg64 aluD;

		if (abSrc != abDst)
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

	void JitOps::op_Extractu_CoS2(TWord op)
	{
		const bool abDst = getFieldValue<Extractu_CoS2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_CoS2, Field_s>(op);

		const auto widthOffset = getOpWordB();
		const auto width = (widthOffset >> 12) & 0x3f;
		const auto offset = widthOffset & 0x3f;

		const auto mask = g_alu_max_56_u >> (56 - width);

		AluReg d(m_block, abDst, false, abSrc != abDst);

		if (abSrc != abDst)
			m_dspRegs.getALU(d, abSrc);

		m_asm.shr(d, asmjit::Imm(offset));
		m_asm.and_(d, asmjit::Imm(mask));

		ccr_clear(CCR_C);
		ccr_clear(CCR_V);
		ccr_dirty(abDst, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Inc(TWord op)
	{
		const auto ab = getFieldValue<Dec, Field_d>(op);
		AluRef r(m_block, ab);

		m_asm.shl(r, asmjit::Imm(8));		// shift left by 8 bits to enable using the host carry bit

#ifdef HAVE_ARM64
		m_asm.adds(r, r, asmjit::Imm(0x100));
#else
		m_asm.add(r, asmjit::Imm(0x100));
#endif
		ccr_update_ifCarry(CCRB_C);

		m_asm.shr(r, asmjit::Imm(8));

		ccr_clear(CCR_V);					// never set in the simulator, even when wrapping around. Carry is set instead

		ccr_dirty(ab, r, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Insert_S1S2(TWord op)
	{
		const auto D   = getFieldValue<Insert_S1S2, Field_D>(op);
		const auto qqq = getFieldValue<Insert_S1S2, Field_qqq>(op);
		const auto sss = getFieldValue<Insert_S1S2, Field_SSS>(op);

		DspValue src(m_block, UsePooledTemp);
		DspValue co(m_block);

		decode_qqq_read(src, qqq);
		decode_sss_read(co, sss);

		alu_insert(D, src, co);
	}

	void JitOps::op_Insert_CoS2(TWord op)
	{
		const auto D   = getFieldValue<Insert_CoS2, Field_D>(op);
		const auto qqq = getFieldValue<Insert_CoS2, Field_qqq>(op);

		DspValue src(m_block, UsePooledTemp);
		DspValue co(m_block);

		decode_qqq_read(src, qqq);
		getOpWordB(co);

		alu_insert(D, src, co);
	}

	void JitOps::op_Lsl_D(TWord op)
	{
		const auto D = getFieldValue<Lsl_D, Field_D>(op);
		DspValue shiftAmount(m_block);
		shiftAmount.set(1, DspValue::Immediate24);
		alu_lsl(D, shiftAmount);
	}

	void JitOps::op_Lsl_ii(TWord op)
	{
		const auto iii = getFieldValue<Lsl_ii, Field_iiiii>(op);
		const auto D = getFieldValue<Lsl_ii, Field_D>(op);

		DspValue shiftAmount(m_block);
		shiftAmount.set(iii, DspValue::Immediate24);
		alu_lsl(D, shiftAmount);
	}

	void JitOps::op_Lsl_SD(TWord op)
	{
		const auto sss   = getFieldValue<Lsl_SD,Field_sss>(op);
		const auto abDst = getFieldValue<Lsl_SD,Field_D>(op);

		DspValue shiftAmount(m_block);
		decode_sss_read(shiftAmount, sss);

		alu_lsl(abDst, shiftAmount);
	}

	void JitOps::op_Lsr_D(TWord op)
	{
		const auto D = getFieldValue<Lsr_D, Field_D>(op);

		DspValue shiftAmount(m_block);
		shiftAmount.set(1, DspValue::Immediate24);
		alu_lsr(D, shiftAmount);
	}

	void JitOps::op_Lsr_ii(TWord op)
	{
		const auto iii = getFieldValue<Lsr_ii, Field_iiiii>(op);
		const auto abDst = getFieldValue<Lsr_ii, Field_D>(op);

		DspValue shiftAmount(m_block);
		shiftAmount.set(iii, DspValue::Immediate24);
		alu_lsr(abDst, shiftAmount);
	}

	void JitOps::op_Lsr_SD(TWord op)
	{
		const auto sss   = getFieldValue<Lsr_SD,Field_sss>(op);
		const auto abDst = getFieldValue<Lsr_SD,Field_D>(op);

		DspValue shiftAmount(m_block);
		decode_sss_read(shiftAmount, sss);

		alu_lsr(abDst, shiftAmount);
	}

	void JitOps::op_Max(TWord)
	{
		AluRef a(m_block, 0, true);
		AluReg b(m_block, 1, false);

		signextend56to64(a);
		signextend56to64(b);

		m_asm.cmp(a,b);

#ifdef HAVE_ARM64
		m_asm.csel(b, a, b, asmjit::arm::CondCode::kGE);
#else
		m_asm.cmovge(b, a);
#endif
		ccr_update_ifLess(CCRB_C);

		m_dspRegs.mask56(a);
		m_dspRegs.mask56(b);
	}

	void JitOps::op_Maxm(TWord)
	{
		AluReg a(m_block, 0, true);
		AluReg b(m_block, 1, true);

		signextend56to64(a);
		signextend56to64(b);

#ifdef HAVE_ARM64
		m_asm.negs(a, a);										// negate
		m_asm.cneg(a, a, asmjit::arm::CondCode::kNegative);	// negate again if now negative
		m_asm.negs(b, b);										// negate
		m_asm.cneg(b, b, asmjit::arm::CondCode::kNegative);	// negate again if now negative
#else
		RegGP temp(m_block);
		m_asm.mov(temp, a);
		m_asm.neg(temp);				// negate
		m_asm.cmovge(a, temp);			// if now positive, use it

		m_asm.mov(temp, b);			// same for b
		m_asm.neg(temp);
		m_asm.cmovge(b, temp);
		temp.release();
#endif

		AluRef refA(m_block, 0, true, false);
		AluRef refB(m_block, 1, true, true);

		m_asm.cmp(a,b);
#ifdef HAVE_ARM64
		m_asm.csel(refB, refA, refB, asmjit::arm::CondCode::kGE);
#else
		m_asm.cmovge(refB, refA);
#endif
		ccr_update_ifLess(CCRB_C);

		a.release();
		b.release();

		m_dspRegs.mask56(refB);
	}

	void JitOps::op_Mpyi(TWord op)
	{
		const bool	ab = getFieldValue<Mpyi, Field_d>(op);
		const bool	negate = getFieldValue<Mpyi, Field_k>(op);
		const TWord qq = getFieldValue<Mpyi, Field_qq>(op);

		DspValue s(m_block);
		getOpWordB(s);

		DspValue reg(m_block);
		decode_qq_read(reg, qq, true);

		alu_mpy(ab, reg, s, negate, false, false, false, false);
	}

	void JitOps::op_Neg(TWord op)
	{
		const auto D = getFieldValue<Neg, Field_d>(op);

		AluRef r(m_block, D);

		m_asm.neg(r);
		m_dspRegs.mask56(r);

		ccr_clear(CCR_V);

		ccr_dirty(D, r, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Nop(TWord op)
	{
	}

	void JitOps::op_Normf(TWord op)
	{
		// if S[23] == 0
		//     ASR S,D
		// else
		//     ASL -S,D

		const auto sss = getFieldValue(Normf, Field_sss, op);
		const auto D = getFieldValue(Normf, Field_D, op);

		AluRef alu(m_block, D, true, true);
		alu.get();	// force to lock already now

		DspValue src(m_block);
		decode_sss_read(src, sss);

		const ShiftReg shifter(m_block);
		m_asm.mov(r32(shifter), r32(src));

		const auto asl = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.bitTest(shifter, 23);
		m_asm.jnz(asl);

		// ASR
		alu_asr(D, D, &shifter);
		m_asm.jmp(end);

		// ASL
		m_asm.bind(asl);
		m_asm.shl(r32(shifter), asmjit::Imm(8));
		m_asm.neg(shifter);
		m_asm.shr(r32(shifter), asmjit::Imm(8));
		alu_asl(D,D, &shifter);

		m_asm.bind(end);
	}

	void JitOps::op_Or_SD(TWord op)
	{
		const auto D = getFieldValue<Or_SD, Field_d>(op);
		const auto JJ = getFieldValue<Or_SD, Field_JJ>(op);
		DspValue r(m_block);
		decode_JJ_read(r, JJ);
		alu_or(D, r);
	}

	void JitOps::op_Or_xx(TWord op)
	{
		const auto ab		= getFieldValue<Or_xx,Field_d>(op);
		const TWord xxxx	= getFieldValue<Or_xx,Field_iiiiii>(op);

		DspValue r(m_block, xxxx, DspValue::Immediate24);
		alu_or(ab, r);
	}

	void JitOps::op_Or_xxxx(TWord op)
	{
		const auto ab = getFieldValue<Or_xxxx,Field_d>(op);

		DspValue r(m_block);
		getOpWordB(r);
		alu_or(ab, r);
	}

	void JitOps::op_Ori(TWord op)
	{
		const auto ee = getFieldValue<Ori, Field_EE>(op);
		const auto iiiiii = getFieldValue<Ori, Field_iiiiiiii>(op);

		const auto ccr = ee == 1;

		if(ccr)
		{
			// set all CCR bits that are set in the immediate value
			for (uint32_t i = 0; i < 8; ++i)
			{
				const auto mask = (1 << i);

				if ((iiiiii & mask))
				{
					ccr_set(static_cast<CCRMask>(mask));
				}
			}

			return;
		}

		RegGP r(m_block);
		decode_EE_read(r, ee);
#ifdef HAVE_ARM64
		{
			const RegGP temp(m_block);
			m_asm.mov(temp, asmjit::Imm(iiiiii));
			m_asm.orr(r, r, temp.get());
		}
#else
		m_asm.or_(r, asmjit::Imm(iiiiii));
#endif
		decode_EE_write(r, ee);
	}

	void JitOps::op_Rnd(TWord op)
	{
		const auto d = getFieldValue<Rnd, Field_d>(op);
		alu_rnd(d);
	}

	void JitOps::op_Sub_SD(TWord op)
	{
		const auto D = getFieldValue<Sub_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Sub_SD, Field_JJJ>(op);

		const auto v = decode_JJJ_read_56(JJJ, !D);
		alu_sub(D, r64(v.get()));
	}

	void JitOps::op_Sub_xx(TWord op)
	{
		const auto ab = getFieldValue<Sub_xx, Field_d>(op);
		const TWord iiiiii = getFieldValue<Sub_xx, Field_iiiiii>(op);

		alu_sub(ab, static_cast<uint8_t>(iiiiii));
	}

	void JitOps::op_Sub_xxxx(TWord op)
	{
		const auto ab = getFieldValue<Sub_xxxx, Field_d>(op);

		RegGP r(m_block);
		const auto opB = signed24To56(getOpWordB());
		m_asm.mov(r64(r), asmjit::Imm(opB));
		alu_sub(ab, r);		// TODO use immediate data
	}

	void JitOps::op_Subr(TWord op)
	{
		errNotImplemented(op);
	}

	void JitOps::op_Tcc_S1D1(TWord op)
	{
		const auto JJJ = getFieldValue<Tcc_S1D1, Field_JJJ>(op);
		const bool ab = getFieldValue<Tcc_S1D1, Field_d>(op);
		const auto cccc = getFieldValue<Tcc_S1D1, Field_CCCC>(op);

		AluRef r(m_block, ab, true, true);

		const DspValue temp = decode_JJJ_read_56(JJJ, !ab);

		m_asm.cmov(decode_cccc(cccc), r64(r), r64(temp));
	}

	void JitOps::op_Tcc_S1D1S2D2(TWord op)
	{
		const auto TTT = getFieldValue<Tcc_S1D1S2D2, Field_TTT>(op);
		const auto JJJ = getFieldValue<Tcc_S1D1S2D2, Field_JJJ>(op);
		const auto ttt = getFieldValue<Tcc_S1D1S2D2, Field_ttt>(op);
		const auto ab = getFieldValue<Tcc_S1D1S2D2, Field_d>(op);
		const auto cccc = getFieldValue<Tcc_S1D1S2D2, Field_CCCC>(op);

		AluRef r(m_block, ab, true, true);
		r.get();	// force load

		const auto temp = decode_JJJ_read_56(JJJ, !ab);

		const DspValue src = makeDspValueRegR(m_block, ttt);
		const DspValue dst = makeDspValueRegR(m_block, TTT, true, true);

		const auto cond = decode_cccc(cccc);

		m_asm.cmov(cond, r64(r), r64(temp));
		m_asm.cmov(cond, r32(dst), r32(src));
	}

	void JitOps::op_Tcc_S2D2(TWord op)
	{
		const auto TTT  = getFieldValue<Tcc_S2D2, Field_TTT>(op);
		const auto ttt  = getFieldValue<Tcc_S2D2, Field_ttt>(op);
		const auto cccc = getFieldValue<Tcc_S2D2, Field_CCCC>(op);

		if (TTT == ttt)
			return;

		const DspValue src = makeDspValueRegR(m_block, ttt, true, false);
		const DspValue dst = makeDspValueRegR(m_block, TTT, true, true);

		m_asm.cmov(decode_cccc(cccc), r32(dst), r32(src));
	}

	void JitOps::op_Tfr(TWord op)
	{
		const auto D = getFieldValue<Tfr, Field_d>(op);
		const auto JJJ = getFieldValue<Tfr, Field_JJJ>(op);

		AluRef ref(m_block, D, false, true);
		decode_JJJ_read_56(ref.get(), JJJ, !D);
	}

	void JitOps::op_Tst(TWord op)
	{
		const auto D = getFieldValue<Tst, Field_d>(op);

		AluRef d(m_block, D, true, false);
		ccr_dirty(D, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
		ccr_clear(CCR_V);
	}
}
