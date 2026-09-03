#include "jitdspregpool.h"
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
		if(isSixteenBitArithmetic())
		{
			// X1[23..8] -> bits 47..32, X0[23..8] -> bits 31..16 (FM figure 3-10)
			const RegGP t(m_block);
			m_asm.mov(r64(t), _dst);
			m_asm.and_(r32(t), asmjit::Imm(0xffff00));
			m_asm.shl(r64(t), asmjit::Imm(8));
			m_asm.shr(_dst, asmjit::Imm(32));
			m_asm.shl(_dst, asmjit::Imm(32));
			m_asm.or_(_dst, r64(t));
		}
		signextend48to56(_dst);
		if constexpr (g_leftAlignedAlu)
			m_asm.shl(_dst, asmjit::Imm(8));	// the result feeds ALU arithmetic, so match the ALU representation
	}

	void JitOps::op_Abs(TWord op)
	{
		const auto ab = getFieldValue<Abs, Field_d>(op);

		AluRef ra(m_block, ab);							// Load ALU

		aluExtendTo64(ra);				// extend to 64 bits

		alu_abs(ra);

		aluRestoreFrom64(ra);

	//	sr_v_update(d);
	//	sr_l_update_by_v();
		ccr_dirty(ab, ra, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_add(const TWord _ab, const JitReg64& _v)
	{
		AluRef alu(m_block, _ab);

		if constexpr (g_leftAlignedAlu)
		{
			// Left-aligned: the carry out of the 56-bit accumulator IS the host carry flag. The batch
			// update clobbers EFLAGS, so it has to be emitted before the operation rather than after.
			if(!m_disableCCRUpdates)
			{
				CcrBatchUpdate bu(*this, CCR_C, CCR_V);
#ifdef HAVE_ARM64
				m_asm.adds(alu, alu, _v);
#else
				m_asm.add(alu, _v);
#endif
				ccr_update_ifCarry(CCRB_C);
			}
			else
			{
				m_asm.add(alu, _v);
			}
		}
		else
		{
		m_asm.add(alu, _v);

		if(!m_disableCCRUpdates)
		{
			CcrBatchUpdate bu(*this, CCR_C, CCR_V);

			copyBitToCCR(alu, 56, CCRB_C);

//			ccr_clear(CCR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops
		}
		}

		// see alu_mpy: adding two values that are clean by the accumulator invariant stays clean
		if constexpr (!g_leftAlignedAlu)
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

		if constexpr (g_leftAlignedAlu)
		{
			// Left-aligned: the carry out of the 56-bit accumulator IS the host carry flag. The batch
			// update clobbers EFLAGS, so it has to be emitted before the operation rather than after.
			if(!m_disableCCRUpdates)
			{
				CcrBatchUpdate bu(*this, CCR_C, CCR_V);
#ifdef HAVE_ARM64
				m_asm.subs(alu, alu, _v);
				ccr_update_ifNotCarry(CCRB_C);	// ARM carry means unsigned >=, inverted vs 56k/x64
#else
				m_asm.sub(alu, _v);
				ccr_update_ifCarry(CCRB_C);
#endif
			}
			else
			{
				m_asm.sub(alu, _v);
			}
		}
		else
		{
		m_asm.sub(alu, _v);

		if(!m_disableCCRUpdates)
		{
			CcrBatchUpdate bu(*this, CCR_C, CCR_V);

			copyBitToCCR(alu, 56, CCRB_C);

//			ccr_clear(CCR_V); batch cleared
		}
		}

		// see alu_mpy: adding two values that are clean by the accumulator invariant stays clean
		if constexpr (!g_leftAlignedAlu)
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
		m_asm.mov(_r, asmjit::Imm(static_cast<uint64_t>(_i) << (24 + (g_leftAlignedAlu ? 8 : 0))));
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

		aluSignextendTo64(aluD);

#ifdef HAVE_ARM64
		m_asm.lsl(aluD, aluD, asmjit::Imm(1));
#else
		m_asm.sal(aluD, asmjit::Imm(1));
#endif
		{
			AluReg aluS(m_block, ab ? 0 : 1, true);

			aluSignextendTo64(aluS);

#ifdef HAVE_ARM64
			m_asm.adds(aluD, aluD, aluS.get());
#else
			m_asm.add(aluD, aluS.get());
#endif
		}

		ccr_update_ifCarry(CCRB_C);

		// D = 2 * D + S: the shift is to the LEFT, so the spare low byte stays zero, and adding another
		// accumulator keeps it that way. Contrast op_Addr below, which shifts right and does need the mask.
		if constexpr (!g_leftAlignedAlu)
			m_dspRegs.mask56(aluD);

		ccr_clear(CCR_V);	// TODO: Set if overflow has occurred in the A or B result or the MSB of the destination operand is changed as a result of the instruction�s left shift.
		ccr_dirty(ab, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Addr(TWord op)
	{
		// D = D/2 + S

		const auto ab = getFieldValue<Addl, Field_d>(op);

		AluReg aluD(m_block, ab);

		aluExtendTo64(aluD);
		m_asm.sar(aluD, asmjit::Imm(1));
		aluRestoreFrom64(aluD);		// discards the bit shifted out, left-aligned or not

		{
			AluRef aluS(m_block, ab ? 0 : 1, true, false);
#ifdef HAVE_ARM64
			m_asm.adds(aluD, aluD, aluS.get());
#else
			m_asm.add(aluD, aluS.get());
#endif

			// Left-aligned the sum can exceed 64 bits, so the 57th bit is the host carry rather than
			// something a compare against the 56-bit maximum could still see. Capture it immediately:
			// leaving the scope may emit a register release and clobber EFLAGS.
			if constexpr (g_leftAlignedAlu)
				ccr_update_ifCarry(CCRB_C);
		}

		if constexpr (!g_leftAlignedAlu)
		{
			{
				const RegScratch aluMax(m_block);
				m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
				m_asm.cmp(aluD, aluMax);
			}

			ccr_update_ifGreater(CCRB_C);
		}

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
		m_asm.and_(r.get(), asmjit::Imm(0x3f));	// "In the control register S1: bits 5�0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
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
		m_asm.and_(r.get(), asmjit::Imm(0x3f));	// "In the control register S1: bits 5�0 (LSB) are used as the #ii field, and the rest of the register is ignored." TODO: this is missing in the interpreter!
		const ShiftReg shifter(m_block);
		m_asm.mov(r32(shifter), r32(r));
		r.release();
		alu_asr(abSrc, abDst, &shifter);
	}

	void JitOps::alu_cmp(TWord ab, const JitReg64& _v, bool _magnitude)
	{
		AluReg d(m_block, ab, true);

		// Both operands already carry the ALU representation when left-aligned: d is an accumulator and
		// _v comes from decode_JJJ_read_56, which produces ALU-aligned values.
		if constexpr (!g_leftAlignedAlu)
		{
			m_asm.sal(d.get(), asmjit::Imm(8));
			m_asm.sal(_v, asmjit::Imm(8));
		}

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

		if constexpr (!g_leftAlignedAlu)
		{
			m_asm.shr(d, asmjit::Imm(8));
			m_asm.shr(_v, asmjit::Imm(8));
		}

		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_eor(const TWord ab, DspValue& _v)
	{
		AluRef r(m_block, ab);

		// we cannot specify an arbitrary immediate shifted up 24 bits so we need to do convert it to a register first
		if(_v.isImm24())
			_v.toTemp();
#ifdef HAVE_ARM64
		m_asm.eor(r, r, r64(_v.get()), asmjit::arm::lsl(24 + g_aluBitOffset));
#else
		m_asm.shl(r64(_v.get()), asmjit::Imm(24 + g_aluBitOffset));
		m_asm.xor_(r, r64(_v.get()));
#endif
		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(r);

		const RegGP t(m_block);
#ifdef HAVE_ARM64
		m_asm.ubfx(r64(t), r64(r), asmjit::Imm(24 + g_aluBitOffset), asmjit::Imm(24));
		m_asm.test_(t);
#else
		m_asm.ror(t.get(), r, 24 + g_aluBitOffset);
		m_asm.test(t, asmjit::Imm(0xffffff));
#endif
		ccr_update_ifZero(CCRB_Z);

		ccr_clear(CCR_V);
	}

	void JitOps::alu_mpy(TWord ab, DspValue& _s1, DspValue& _s2, bool _negate, bool _accumulate, bool _s1Unsigned, bool _s2Unsigned, bool _round, const uint32_t _s1Shift)
	{
		// _s1Shift is how much of g_mpyProductShift the caller already folded into _s1
		assert(_s1Shift <= g_mpyProductShift);

		if (_s2.isImmediate())
		{
			// The immediate is a SIGNED 24-bit fractional value, but DspValue stores it raw.
			// The interpreter sign extends it through TReg24; without the same here, every
			// immediate >= $800000 multiplies as a large positive instead of a negative - on
			// x64 through imul's constant, on aarch64 through the register smull materialises.
			// The Virus firmware only uses `maci #>$7fdf3b`, below that threshold, which is why
			// the two engines agreed until now.
			_s2.imm() = TReg24(_s2.imm24()).signextend<int64_t>();
		}
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
			aluSignextendTo64(d);
			// fractional multiplication requires one post-shift; left-aligned the product is scaled here too
			m_asm.add(d, d, r64(_s1), asmjit::arm::lsl(1 + g_aluBitOffset));
		}
		else
		{
			// fractional multiplication requires one post-shift; left-aligned the product is scaled here too
			m_asm.lsl(d, r64(_s1), asmjit::Imm(1 + g_aluBitOffset));
		}
#else
		if(_s2.isImmediate())
		{
			// Same accounting as the register path: g_mpyProductShift of scale is needed, minus
			// whatever the caller already folded into _s1's sign extension. The constant can
			// absorb one bit of it for free (imm * 2 still fits an imm32); the rest is a shift,
			// and when _s1 arrives pre-scaled there is nothing left to do at all.
			const auto remainingShift = g_mpyProductShift - _s1Shift;

			const int64_t i = _s2.imm() << (remainingShift ? 1 : 0);	// sign extended above

			if(_negate)
			{
				m_asm.imul(r64(_s1), asmjit::Imm(-i));
			}
			else
			{
				if (i > 0 && asmjit::Support::isPowerOf2(i))
				{
					const auto shift = static_cast<uint32_t>(log2(i));
					m_asm.shl(r64(_s1), asmjit::Imm(shift));
				}
				else
				{
					m_asm.imul(r64(_s1), asmjit::Imm(i));
				}
			}

			if (remainingShift > 1)
				m_asm.shl(r64(_s1), asmjit::Imm(remainingShift - 1));

			if (_accumulate)
			{
				aluSignextendTo64(d);
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

			// The product needs scaling by one for the fractional multiply plus g_aluBitOffset for
			// the left-aligned accumulator. decode_QQQQ_read can bake that into the sign extension
			// of _s1 for free (see signextend24to64), in which case nothing is left to do here and
			// the accumulate is a plain add instead of a shift plus a scaled lea.
			const auto remainingShift = g_mpyProductShift - _s1Shift;

			if (remainingShift)
				m_asm.shl(r64(_s1), asmjit::Imm(remainingShift));

			if (_accumulate)
			{
				aluSignextendTo64(d);

				if (_negate)
					m_asm.sub(d, r64(_s1));
				else
					m_asm.add(d, r64(_s1));
			}
			else
			{
				if (_negate)
					m_asm.neg(r64(_s1));
				m_asm.mov(d, r64(_s1));
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

			// Left-aligned, mask56 only clears the low byte - it cannot trim an overflow, because the 56
			// bits already occupy 63..8. After a multiply that byte is zero by construction: the product
			// is shifted up by 8, doubling keeps it zero, and the accumulator it is added to is clean by
			// the same invariant.
			if constexpr (!g_leftAlignedAlu)
			{
				if (canOverflow || _negate)
					m_dspRegs.mask56(d);
			}
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

			decode_QQQQ_read(s1, true, s2, true, qqq, g_mpyOperandShift);

			alu_mpy(ab, s1, s2, negative, mulAcc, false, false, round, g_mpyOperandShift);
		}
	}

	void JitOps::alu_or(TWord ab, DspValue& _v)
	{
		AluRef r(m_block, ab);

		// we cannot specify an arbitrary immediate shifted up 24 bits so we need to do convert it to a register first
		if(_v.isImm24())
			_v.toTemp();

		m_asm.shl(r64(_v.get()), asmjit::Imm(24 + g_aluBitOffset));
		m_asm.or_(r, r64(_v.get()));

		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(r);

		const RegGP t(m_block);
#ifdef HAVE_ARM64
		m_asm.ubfx(r64(t), r64(r), asmjit::Imm(24 + g_aluBitOffset), asmjit::Imm(24));
		m_asm.test_(t);
#else
		m_asm.ror(t.get(), r, 24 + g_aluBitOffset);
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
		m_asm.mov(v, asmjit::Imm(g_leftAlignedAlu ? (r56.var << 8) : r56.var));
		alu_cmp(D, v, false);
	}

	void JitOps::op_Cmp_xxxxS2(TWord op)
	{
		const auto D = getFieldValue<Cmp_xxxxS2, Field_d>(op);

		const TReg24 s(signextend<int, 24>(getOpWordB()));

		TReg56 r56;
		convert(r56, s);

		const RegGP v(m_block);
		m_asm.mov(v, asmjit::Imm(g_leftAlignedAlu ? (r56.var << 8) : r56.var));

		alu_cmp(D, v, false);
	}

	void JitOps::op_Cmpm_S1S2(TWord op)
	{
		const auto D = getFieldValue<Cmpm_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmpm_S1S2, Field_JJJ>(op);
		const auto r = decode_JJJ_read_56(JJJ, !D);
		alu_cmp(D, r64(r.get()), true);
	}

	void JitOps::op_Cmpu_S1S2(TWord op)
	{
		const auto D = getFieldValue<Cmpu_S1S2, Field_d>(op);
		const auto ggg = getFieldValue<Cmpu_S1S2, Field_ggg>(op);

		// ggg only defines 0 (the other accumulator) and 4..7 (x0, y0, x1, y1). Those encodings are
		// identical to the ones JJJ uses, so the existing decoder covers every valid CMPU operand.
		assert((ggg == 0 || ggg >= 4) && "invalid ggg value for CMPU");

		// Nothing needs to be flushed first: the batch below clears the dirty flags of every bit CMPU
		// writes, and CMPU never becomes the last modifying ALU op, so regLastModAlu keeps pointing at
		// the previous result. E and U therefore stay lazy across the compare and are still evaluated
		// from that previous result, which is exactly what "unchanged by the instruction" means.

		const auto v = decode_JJJ_read_56(ggg, !D);

		AluReg d(m_block, D, true);		// read only, CMPU does not store a result

		const RegGP s1(m_block);
		m_asm.mov(r64(s1), r64(v.get()));

		// CMPU compares two 48 bit UNSIGNED operands, the accumulator extension takes no part in it.
		// Shifting both operands up so that bit 47 lands on bit 63 hands the whole comparison to the
		// host flags: borrow, zero and sign then all describe the 48 bit result, and the bits below are
		// zero so they can neither produce a borrow of their own nor disturb the zero test.
		constexpr auto shift = 16 - g_aluBitOffset;

		m_asm.shl(r64(d), asmjit::Imm(shift));
		m_asm.shl(r64(s1), asmjit::Imm(shift));

		{
			// One batch clears C, V, Z and N up front so that each of them can be OR'ed in below. V is
			// "always cleared" and so needs nothing beyond that clear, and having C pre-cleared is what
			// lets the carry be folded in with a single adc.
			CcrBatchUpdate u(*this, static_cast<CCRMask>(CCR_C | CCR_V | CCR_Z | CCR_N));

#ifdef HAVE_ARM64
			m_asm.subs(r64(d), r64(d), r64(s1));
			ccr_update_ifNotCarry(CCRB_C);	// on ARM carry means unsigned >=, while it means unsigned < on 56k and intel
#else
			m_asm.sub(r64(d), r64(s1));
			ccr_update_ifCarry(CCRB_C);
#endif

			m_asm.test_(r64(d));
			ccr_update_ifZero(CCRB_Z);		// "set if bits 47-0 of the result are 0"

			// back into the ALU domain so that the existing bit 47 helper can be used for N
			m_asm.shr(r64(d), asmjit::Imm(shift));
			ccr_n_update_by47(r64(d));
		}
	}

	void JitOps::op_Dec(TWord op)
	{
		const auto ab = getFieldValue<Dec, Field_d>(op);
		AluRef r(m_block, ab);

		aluExtendTo64(r);	// reach the 64 bit boundary to use the host carry bit (free when left-aligned)

#ifdef HAVE_ARM64
		m_asm.subs(r, r, asmjit::Imm(0x100));
		ccr_update_ifNotCarry(CCRB_C);
#else
		m_asm.sub(r, asmjit::Imm(0x100));
		ccr_update_ifCarry(CCRB_C);
#endif

		aluRestoreFrom64(r);
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
		// One shift, not two: the fractional post-shift and the left-alignment scale are both
		// applied to the same register, so they combine. Note the accumulator's own sar by 24
		// below already yields the correct left-aligned form, since (d >> 24) << 8 == (d << 8) >> 24.
		m_asm.sal(r64(s1), asmjit::Imm(g_mpyProductShift));

		if (negate)
			m_asm.neg(r64(s1));

		AluRef d(m_block, ab);

		aluSignextendTo64(d);
		m_asm.sar(d, asmjit::Imm(24));

		m_asm.add(d, r64(s1));
		s1.release();

		const auto& dOld = r64(s2);
		m_asm.mov(dOld, d.get());

		m_dspRegs.mask56(d);

		// Update SR
		// detect overflow by sign-extending the actual result and comparing VS the non-sign-extended one. We've got overflow if they are different
		m_asm.cmp(dOld, d.get());

		ccr_vl_update_ifNotZero();

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
		const auto ab = getFieldValue<Eor_xx, Field_d>(op);
		const auto xxxx = getFieldValue<Eor_xx, Field_iiiiii>(op);
		DspValue r(m_block, xxxx, DspValue::Immediate24);
		alu_eor(ab, r);
	}

	void JitOps::op_Eor_xxxx(TWord op)
	{
		const auto ab = getFieldValue<Eor_xxxx, Field_d>(op);

		DspValue r(m_block);
		getOpWordB(r);
		alu_eor(ab, r);
	}

	void JitOps::decodeBitfieldControl(const DspValue& _control, const JitRegGP& _width, const JitRegGP& _offset)
	{
		_control.copyTo(_width, 24);
		_control.copyTo(_offset, 24);

		if(m_block.getMode() && m_block.getMode()->testSR(SRB_SA))
		{
			m_asm.shr(r32(_width), asmjit::Imm(16));
			m_asm.shr(r32(_offset), asmjit::Imm(8));
		}
		else
			m_asm.shr(r32(_width), asmjit::Imm(12));
		m_asm.and_(r32(_width), asmjit::Imm(0x3f));
		m_asm.and_(r32(_offset), asmjit::Imm(0x3f));
	}

	void JitOps::alu_extract(const TWord abDst, const TWord abSrc, DspValue& widthOffset, const bool signExtend)
	{
		ccr_clear(CCR_C);
		ccr_clear(CCR_V);

		RegGP width(m_block);
		ShiftReg shift(m_block);
		decodeBitfieldControl(widthOffset, width.get(), shift.get());
		widthOffset.release();

		AluReg s(m_block, abSrc, abSrc != abDst);
		if constexpr (g_leftAlignedAlu)
			m_asm.shr(s, asmjit::Imm(8));

#ifdef HAVE_X86_64
		if (JitEmitter::hasBMI2())
			m_asm.shrx(s, s, shift.get());
		else
#endif
			m_asm.shr(s, shiftOperand(shift.get()));

		const auto zeroWidth = m_asm.newLabel();
		const auto extracted = m_asm.newLabel();
		m_asm.test_(r32(width));
		m_asm.jz(zeroWidth);
		if(signExtend)
		{
			// Shift the field's sign bit to the host sign bit and arithmetic-shift it back.
			m_asm.mov(r32(shift), r32(width));
			m_asm.neg(r32(shift));
			m_asm.add(r32(shift), asmjit::Imm(64));
#ifdef HAVE_ARM64
			m_asm.shl(s, r64(shift));
			m_asm.asr(s, s, r64(shift));
#else
			m_asm.shl(s, shift.get().r8());
			m_asm.sar(s, shift.get().r8());
#endif
		}
		else
		{
			const RegGP mask(m_block);
			m_asm.mov(r64(mask), asmjit::Imm(1));
			m_asm.mov(r32(shift), r32(width));
#ifdef HAVE_ARM64
			m_asm.shl(r64(mask), r64(shift));
#else
			m_asm.shl(r64(mask), shift.get().r8());	// x86 shift counts must live in cl
#endif
			m_asm.dec(r64(mask));
			m_asm.and_(s, r64(mask));
		}
		m_asm.jmp(extracted);
		m_asm.bind(zeroWidth);
		m_asm.xor_(s, s);
		m_asm.bind(extracted);
		width.release();
#ifndef HAVE_X86_64
		shift.release();
#endif

		if constexpr (g_leftAlignedAlu)
			m_asm.shl(s, asmjit::Imm(8));

		JitReg64 aluD;
		if (abSrc != abDst)
		{
			AluRef d(m_block, abDst, false, true);
			m_asm.mov(d, s.get());
			aluD = d.get();
		}
		else
			aluD = s.get();

		ccr_dirty(abDst, aluD, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::op_Extract_S1S2(TWord op)
	{
		const auto sss = getFieldValue<Extract_S1S2, Field_SSS>(op);
		const bool abDst = getFieldValue<Extract_S1S2, Field_D>(op);
		const bool abSrc = getFieldValue<Extract_S1S2, Field_s>(op);
		DspValue widthOffset(m_block);
		decode_sss_read(widthOffset, sss);
		alu_extract(abDst, abSrc, widthOffset, true);
	}

	void JitOps::op_Extract_CoS2(TWord op)
	{
		const bool abDst = getFieldValue<Extract_CoS2, Field_D>(op);
		const bool abSrc = getFieldValue<Extract_CoS2, Field_s>(op);
		DspValue widthOffset(m_block, getOpWordB(), DspValue::Immediate24);
		alu_extract(abDst, abSrc, widthOffset, true);
	}

	void JitOps::op_Extractu_S1S2(TWord op)
	{
		const auto sss = getFieldValue<Extractu_S1S2, Field_SSS>(op);
		const bool abDst = getFieldValue<Extractu_S1S2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_S1S2, Field_s>(op);

		DspValue widthOffset(m_block);
		decode_sss_read(widthOffset, sss);
		alu_extract(abDst, abSrc, widthOffset, false);
	}

	void JitOps::op_Extractu_CoS2(TWord op)
	{
		const bool abDst = getFieldValue<Extractu_CoS2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_CoS2, Field_s>(op);

		DspValue widthOffset(m_block, getOpWordB(), DspValue::Immediate24);
		alu_extract(abDst, abSrc, widthOffset, false);
	}

	void JitOps::op_Inc(TWord op)
	{
		const auto ab = getFieldValue<Dec, Field_d>(op);
		AluRef r(m_block, ab);

		aluExtendTo64(r);		// reach the 64 bit boundary to use the host carry bit (free when left-aligned)

#ifdef HAVE_ARM64
		m_asm.adds(r, r, asmjit::Imm(0x100));
#else
		m_asm.add(r, asmjit::Imm(0x100));
#endif
		ccr_update_ifCarry(CCRB_C);

		aluRestoreFrom64(r);

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
		// MAX only writes the destination accumulator B; A is a read-only source.
		// Take A as a read-only temp copy (NOT an AluRef, which defaults to write=true):
		// a writeback of A here gets latched and, when MAX carries a parallel move into A
		// (e.g. the coef builder's `max a,b  x1,a`, $20AE1D), the deferred A
		// writeback clobbers that move in the parallel-op epilog. See unittest max_parallel().
		AluReg a(m_block, 0, true);
		AluReg b(m_block, 1, false);

		aluSignextendTo64(a);
		aluSignextendTo64(b);

		m_asm.cmp(a,b);

#ifdef HAVE_ARM64
		m_asm.csel(b, a, b, asmjit::arm::CondCode::kGE);
#else
		m_asm.cmovge(b, a);
#endif
		ccr_update_ifLess(CCRB_C);

		m_dspRegs.mask56(b);
	}

	void JitOps::op_Maxm(TWord)
	{
		AluReg a(m_block, 0, true);
		AluReg b(m_block, 1, true);

		aluSignextendTo64(a);
		aluSignextendTo64(b);

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
		decode_qq_read(reg, qq, true, g_mpyOperandShift);

		alu_mpy(ab, reg, s, negate, false, false, false, false, g_mpyOperandShift);
	}

	void JitOps::op_Mpyri(TWord op)
	{
		// MPYRI is MPYI with rounding, which alu_mpy applies itself
		const bool	ab = getFieldValue<Mpyri, Field_d>(op);
		const bool	negate = getFieldValue<Mpyri, Field_k>(op);
		const TWord qq = getFieldValue<Mpyri, Field_qq>(op);

		DspValue s(m_block);
		getOpWordB(s);

		DspValue reg(m_block);
		decode_qq_read(reg, qq, true, g_mpyOperandShift);

		alu_mpy(ab, reg, s, negate, false, false, false, true, g_mpyOperandShift);
	}

	void JitOps::op_Maci_xxxx(TWord op)
	{
		const bool	ab = getFieldValue<Maci_xxxx, Field_d>(op);
		const bool	negate = getFieldValue<Maci_xxxx, Field_k>(op);
		const TWord qq = getFieldValue<Maci_xxxx, Field_qq>(op);

		DspValue s(m_block);
		getOpWordB(s);

		DspValue reg(m_block);
		decode_qq_read(reg, qq, true, g_mpyOperandShift);

		alu_mpy(ab, reg, s, negate, true, false, false, false, g_mpyOperandShift);
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

	void JitOps::op_Norm(TWord op)
	{
		const auto rrr = getFieldValue<Norm, Field_RRR>(op);
		const auto D = getFieldValue<Norm, Field_d>(op);

		// NORM branches on the E/U/Z values established before this
		// instruction.  Flush any lazy flags first, then keep the accumulator
		// and address register live across all three runtime paths.
		updateDirtyCCR(static_cast<CCRMask>(CCR_E | CCR_U | CCR_Z));
		const DspValue sr(m_block, PoolReg::DspSR, true, false);
		AluRef alu(m_block, D, true, true);
		alu.get();
		DspValue r = makeDspValueRegR(m_block, rrr, true, true);

		// NORM leaves the carry bit unchanged (FM 13-146) but the ASL/ASR helpers write it
		const RegGP savedSR(m_block);
		m_asm.mov(r32(savedSR), r32(sr));

		const auto shiftRight = m_asm.newLabel();
		const auto done = m_asm.newLabel();

		m_asm.bitTest(r32(sr), CCRB_E);
		m_asm.jnz(shiftRight);
		m_asm.bitTest(r32(sr), CCRB_U);
		m_asm.jz(done);
		m_asm.bitTest(r32(sr), CCRB_Z);
		m_asm.jnz(done);

		alu_asl(D, D, nullptr, 1);
		copyBitToCCR(r32(savedSR), CCRB_C, CCRB_C);
		m_asm.dec(r32(r));
		m_dspRegs.maskSC1624(r32(r));
		// Do not defer these flags past the NOP path at the join point.
		updateDirtyCCR();
		m_asm.jmp(done);

		m_asm.bind(shiftRight);
		alu_asr(D, D, nullptr, 1);
		copyBitToCCR(r32(savedSR), CCRB_C, CCRB_C);
		m_asm.inc(r32(r));
		m_dspRegs.maskSC1624(r32(r));
		updateDirtyCCR();

		m_asm.bind(done);
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
		// D = D/2 - S (reverse subtract: subtract source from half of destination)
		const auto ab = getFieldValue<Subr, Field_d>(op);

		AluReg aluD(m_block, ab);

		// D/2: arithmetic shift right by 1, maintaining 56-bit format
		m_asm.sal(aluD, asmjit::Imm(8));
		m_asm.sar(aluD, asmjit::Imm(1));
		m_asm.shr(aluD, asmjit::Imm(8));

		{
			AluRef aluS(m_block, ab ? 0 : 1, true, false);
			m_asm.sub(aluD, aluS.get());
		}

		{
			const RegScratch aluMax(m_block);
			m_asm.mov(aluMax, asmjit::Imm(g_alu_max_56_u));
			m_asm.cmp(aluD, aluMax);
		}

		ccr_update_ifGreater(CCRB_C);

		ccr_dirty(ab, aluD, static_cast<CCRMask>(CCR_E | CCR_U | CCR_N));
		ccr_n_update_by55(aluD);
		ccr_s_update(aluD);
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
