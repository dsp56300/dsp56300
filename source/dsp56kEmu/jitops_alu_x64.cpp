#include "jittypes.h"

#ifdef HAVE_X86_64

#include "opcodecycles.h"
#include "dsp.h"
#include "jitblockruntimedata.h"
#include "jitdspmode.h"
#include "jitops.h"
#include "jitops_mem.inl"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::XY0to56(const JitReg64& _dst, int _xy) const
	{
		const auto src = m_block.dspRegPool().get(_xy ? PoolReg::DspY0 : PoolReg::DspX0, true, false);
		signed24To56(_dst, r64(src));
	}
	void JitOps::XY1to56(const JitReg64& _dst, int _xy) const
	{
		const auto src = m_block.dspRegPool().get(_xy ? PoolReg::DspY1 : PoolReg::DspX1, true, false);
		signed24To56(_dst, r64(src));
	}

	void JitOps::alu_abs(const JitRegGP& _r)
	{
		const RegScratch rb(m_block);

		m_asm.mov(rb, _r);		// Copy to backup location
		m_asm.neg(_r);			// negate
		m_asm.cmovl(_r, rb);	// if now negative, restore its saved value
	}

	void JitOps::alu_and(const TWord ab, DspValue& _v)
	{
		m_asm.shl(r64(_v), asmjit::Imm(24 + g_aluBitOffset));

		AluRef alu(m_block, ab);

		m_asm.test(alu, r64(_v));
		ccr_update_ifZero(CCRB_Z);

		{
			const RegScratch mask(m_block);
			// ones outside the 24-bit field so AND leaves a0/a2 alone; left-aligned the field moves up by 8
			m_asm.mov(mask, asmjit::Imm(g_leftAlignedAlu ? 0xff000000ffffff00 : 0xff000000ffffff));
			m_asm.or_(r64(_v), mask);
			m_asm.and_(alu, r64(_v));
		}

		_v.release();

		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(alu);
		ccr_clear(CCR_V);
	}
	
	void JitOps::alu_asl(const TWord _abSrc, const TWord _abDst, const ShiftReg* _v, TWord _bits/* = 0*/)
	{
		CcrBatchUpdate bu(*this, CCR_C, CCR_V);

		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_dspRegs.getALU(alu.get(), _abSrc);

		// we want to hit the 64 bit boundary to make use of the native carry flag, which a left-aligned
		// accumulator already does - otherwise pre-shift by 8 bit (56 => 64)
		aluExtendTo64(alu);

		const RegGP oldAlu(m_block);
		m_asm.mov(oldAlu, alu);

		if(_v)
			m_asm.sal(alu, _v->get().r8());
		else
			m_asm.sal(alu, asmjit::Imm(_bits));

		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical to the backshifted one
		{
			const RegScratch s(m_block);
			m_asm.mov(s, alu);

			if(_v)
				m_asm.sar(s, _v->get().r8());
			else
				m_asm.sar(s, _bits);

			m_asm.cmp(oldAlu, s);
		}

		ccr_update_ifNotZero(CCRB_V);
		
		aluRestoreFrom64(alu);						// correction for the pre-shift, and keeps the low byte clear

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg* _v, TWord _immediate/* = 0*/)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_dspRegs.getALU(alu, _abSrc);

		CcrBatchUpdate bu(*this, CCR_C, CCR_V);

		aluExtendTo64(alu);
		if(_v)
			m_asm.sar(alu, _v->get().r8());
		else
			m_asm.sar(alu, asmjit::Imm(_immediate));
		// discards the bits shifted below the accumulator - the hardware has no resolution there
		aluRestoreFrom64(alu);

		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag
		
//		ccr_clear(CCR_V);							// cleared by batch update

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_bclr(const DspValue& _dst, const TWord _bit)
	{
		m_asm.btr(_dst.get(), asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	void JitOps::alu_bset(const DspValue& _dst, const TWord _bit)
	{
		m_asm.bts(_dst.get(), asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	void JitOps::alu_bchg(const DspValue& _dst, const TWord _bit)
	{
		m_asm.btc(_dst.get(), asmjit::Imm(_bit));
		ccr_update_ifCarry(CCRB_C);
	}

	void JitOps::alu_lsl(TWord ab, const DspValue& _shiftAmount)
	{
		CcrBatchUpdate bu(*this, static_cast<CCRMask>(CCR_N | CCR_C | CCR_V));
		DspValue d(m_block);
		getALU1(d, ab);
		if(_shiftAmount.isImm24())
		{
			m_asm.shl(r32(d.get()), _shiftAmount.imm24() + 8); // + 8 to use native carry flag
		}
		else
		{
			ShiftReg s(m_block);
			m_asm.mov(r32(s), r32(_shiftAmount.get()));
			m_asm.add(r32(s), asmjit::Imm(8));	// + 8 to use native carry flag
			m_asm.shl(r64(d.get()), s.get().r8());
		}
		ccr_update_ifCarry(CCRB_C);
		m_asm.shr(r32(d.get()), 8);				// revert shift by 8
		ccr_update_ifZero(CCRB_Z);
		copyBitToCCR(d.get(), 23, CCRB_N);
//		ccr_clear(CCR_V);	already cleared above
		setALU1(ab, d);
	}

	void JitOps::alu_lsr(TWord ab, const DspValue& _shiftAmount)
	{
		CcrBatchUpdate bu(*this, static_cast<CCRMask>(CCR_N | CCR_C | CCR_V));
		DspValue d(m_block);
		getALU1(d, ab);
		if(_shiftAmount.isImm24())
		{
			m_asm.shr(r32(d.get()), _shiftAmount.imm24());
		}
		else
		{
			ShiftReg s(m_block);
			m_asm.mov(r32(s), r32(_shiftAmount.get()));
			m_asm.shr(r64(d.get()), s.get().r8());
		}
		ccr_update_ifCarry(CCRB_C);
		m_asm.test_(r32(d.get()));
		ccr_update_ifZero(CCRB_Z);
		copyBitToCCR(d.get(), 23, CCRB_N);
//		ccr_clear(CCR_V);	already cleared above
		setALU1(ab, d);
	}

	void JitOps::alu_rnd(TWord ab, const JitReg64& d, const bool _needsSignextend/* = true*/)
	{
		if(_needsSignextend)
			aluSignextendTo64(d);

		const JitDspMode* mode = m_block.getMode();

//		JitDspMode testMode;
//		testMode.initialize(m_block.dsp());
//		assert(!mode || *mode == testMode);

		if(mode)
		{
			// the rounding position is bit 23 of the 56-bit value, which moves up with the representation
			uint64_t rounder = 0x800000ull << g_aluBitOffset;

			if(mode->testSR(SRB_S1))	rounder >>= 1;
			if(mode->testSR(SRB_S0))	rounder <<= 1;

			{
				const RegGP rounderReg(m_block);
				m_asm.mov(r64(rounderReg), asmjit::Imm(rounder));
				m_asm.add(d, r64(rounderReg));
			}
			rounder <<= 1;

			// mask = all the bits to the right of, and including the rounding position
			auto mask = rounder - 1;

			if(!mode->testSR(SRB_RM))
			{
				// convergent rounding. If all mask bits are cleared

				// then the bit to the left of the rounding position is cleared in the result
				// if (!(_alu.var & mask)) 
				//	_alu.var&=~(rounder<<1);
				rounder = ~rounder;

				{
					const RegScratch aluIfAndWithMaskIsZero(m_block);
					const RegGP maskReg(m_block);
					m_asm.mov(aluIfAndWithMaskIsZero, d);
					// left-aligned these no longer fit a sign-extendable imm32, so materialise them
					m_asm.mov(r64(maskReg), asmjit::Imm(rounder));
					m_asm.and_(aluIfAndWithMaskIsZero, r64(maskReg));
					m_asm.mov(r64(maskReg), asmjit::Imm(mask));
					m_asm.test(d, r64(maskReg));
					m_asm.cmovz(d, aluIfAndWithMaskIsZero.get());
				}
			}
			
			// all bits to the right of and including the rounding position are cleared.
			// _alu.var&=~mask;
			mask = ~mask;
			{
				const RegGP maskReg(m_block);
				m_asm.mov(r64(maskReg), asmjit::Imm(mask));
				m_asm.and_(d, r64(maskReg));
			}
		}
		else
		{
			const RegGP rounder(m_block);

			m_asm.mov(r64(rounder), asmjit::Imm(0x800000ull << g_aluBitOffset));

			const ShiftReg shifter(m_block);
			sr_getBitValue(shifter, SRB_S1);
			m_asm.shr(rounder, shifter.get().r8());
			sr_getBitValue(shifter, SRB_S0);
			m_asm.shl(rounder, shifter.get().r8());

			m_asm.add(d, rounder.get());
			m_asm.shl(rounder, asmjit::Imm(1));

			// mask = all the bits to the right of, and including the rounding position
			const RegGP mask(m_block);
			m_asm.mov(mask, rounder.get());
			m_asm.dec(mask);

			const auto skipNoScalingMode = m_asm.newLabel();

			// if (!sr_test_noCache(SR_RM))
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_RM);
			m_asm.jnz(skipNoScalingMode);
			{
				// convergent rounding. If all mask bits are cleared

				// then the bit to the left of the rounding position is cleared in the result
				// if (!(_alu.var & mask)) 
				//	_alu.var&=~(rounder<<1);
				m_asm.not_(rounder);

				{
					const RegScratch aluIfAndWithMaskIsZero(m_block);
					m_asm.mov(aluIfAndWithMaskIsZero, d);
					m_asm.and_(aluIfAndWithMaskIsZero, rounder.get());
					m_asm.test(d, mask.get());
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

	void JitOps::alu_insert(TWord ab, const DspValue& _src, DspValue& _widthOffset)
	{
		AluRef d(m_block, ab);

		// const auto width = (widthOffset >> 12) & 0x3f;
		const ShiftReg width(m_block);
		_widthOffset.copyTo(width.get(), 24);
		m_asm.shr(width, asmjit::Imm(12));
		m_asm.and_(width, asmjit::Imm(0x3f));

		// const auto mask = (1<<width) - 1;
		const RegGP mask(m_block);
		m_asm.mov(r32(mask), asmjit::Imm(1));
		m_asm.shl(mask, width.get().r8());
		m_asm.dec(mask);

		// const uint64_t offset = widthOffset & 0x3f;
		const auto& offset = width;
		m_asm.mov(r32(offset), r32(_widthOffset.get()));
		m_asm.and_(offset.get(), asmjit::Imm(0x3f));

		// the offset is relative to the 56-bit value; shifting both the value and the mask by the
		// aligned offset places the field correctly in either representation
		if constexpr (g_leftAlignedAlu)
			m_asm.add(offset.get(), asmjit::Imm(8));

		// uint64_t s = src & mask;
		const RegGP s(m_block);
		m_asm.mov(r32(s), r32(_src.get()));
		m_asm.and_(r32(s), r32(mask));

		// s <<= offset;
		m_asm.shl(s.get(), offset.get().r8());

		// d &= ~(static_cast<uint64_t>(mask) << offset);
		m_asm.shl(r64(mask), offset.get().r8());
		m_asm.not_(r64(mask));
		m_asm.and_(d.get(), mask);

		// d |= s;
		m_asm.or_(d, s);

		ccr_clear(CCR_C);
		ccr_clear(CCR_V);
		ccr_dirty(ab, d, static_cast<CCRMask>(CCR_E | CCR_U | CCR_N | CCR_Z));
	}

	void JitOps::op_Btst_ea(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_ea>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_ea>(op), CCRB_C);
	}

	void JitOps::op_Btst_aa(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_aa>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_aa>(op), CCRB_C);
	}

	void JitOps::op_Btst_pp(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_pp>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_pp>(op), CCRB_C);
	}

	void JitOps::op_Btst_qq(TWord op)
	{
		DspValue r(m_block);
		readMem<Btst_qq>(r, op);
		copyBitToCCR(r.get(), getBit<Btst_qq>(op), CCRB_C);
	}

	void JitOps::op_Btst_D(TWord op)
	{
		const auto dddddd = getFieldValue<Btst_D, Field_DDDDDD>(op);

		DspValue r(m_block);
		decode_dddddd_read(r, dddddd);

		copyBitToCCR(r.get(), getBit<Btst_D>(op), CCRB_C);
	}

	void JitOps::op_Clb(TWord op)
	{
		const auto S = getFieldValue(Clb, Field_S, op);
		const auto D = getFieldValue(Clb, Field_D, op);

		AluRef s(m_block, S, true, D == S);
		AluRef d(m_block, D, S == D, true);

		const RegGP t(m_block);
		const RegGP shifted(m_block);
		m_asm.mov(shifted, s);
		if constexpr (!g_leftAlignedAlu)
			m_asm.sal(shifted, 8);	// count from the MSB; left-aligned it is already there

		// this instruction counts the number of equal bits starting at the MSB
		// We can only count leading zeroes, so we invert the source if the MSB is a 1
		m_asm.mov(t, shifted);
		m_asm.not_(t);
		// 'not' does not touch the flags, so set them explicitly. Do not rely on whatever ran before:
		// right-aligned this used to be the 'sal' above, left-aligned there is no shift left to do.
		m_asm.test_(shifted);
		m_asm.cmov(asmjit::x86::CondCode::kNotSign, t, shifted);

		// we want to prevent to have a completely empty register as the BSR result will be UB.
		// This OR will ensure that an empty ALU results in a valid result
		m_asm.or_(r64(t), asmjit::Imm(0xff));

		m_asm.bsr(r64(t), r64(t));					// this does not give us the number of leading zeroes but the bit index of the first one
		m_asm.sub(r32(t), asmjit::Imm(64 - 9 - 1));	// range of DSP result is -47 ... +8

		// special case: if the source alu is 0, the result is 0
		m_asm.test_(s);
		m_asm.cmovz(t,s);

		CcrBatchUpdate ccrBatch(*this, CCR_N, CCR_Z, CCR_V);
		copyBitToCCR(d, 23 + g_aluBitOffset, CCRB_N);

		m_asm.shl(r64(t), asmjit::Imm(24 + g_aluBitOffset));
		ccr_update_ifZero(CCRB_Z);

		m_asm.mov(r64(d), r64(t));
	}

	void JitOps::op_Div(TWord op)
	{
		const auto ab = getFieldValue<Div, Field_d>(op);
		const auto jj = getFieldValue<Div, Field_JJ>(op);

		m_ccrRead |= CCR_C;
		updateDirtyCCR(CCR_C);

		AluRef d(m_block, ab);

		// V and L updates
		// V: Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
		// L: Set if the Overflow bit (V) is set.
		// What we do is we check if bits 55 and 54 of the ALU are not identical (host parity bit cleared) and set V accordingly.
		{
			const RegGP r(m_block);
			m_asm.ror(r, d.get(), 54 + g_aluBitOffset);	// bits 55/54 of the accumulator
			m_asm.and_(r, asmjit::Imm(0x3));
		}

		ccr_vl_update_ifNotParity();

		{
			DspValue s(m_block);
			decode_JJ_read(s, jj);

			m_asm.shl(r64(s), asmjit::Imm(40));
			m_asm.sar(r64(s), asmjit::Imm(16 - g_aluBitOffset));	// land on the ALU field position

			const RegGP addOrSub(m_block);
			m_asm.mov(addOrSub, r64(s));
			m_asm.xor_(addOrSub, d.get());

			{
				const RegScratch sNeg(m_block);
				m_asm.mov(sNeg, r64(s));
				m_asm.neg(sNeg);

				m_asm.bt(addOrSub, asmjit::Imm(55 + g_aluBitOffset));

				m_asm.cmovnc(r64(s), sNeg);
			}

			m_asm.add(d, d);

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_C));
			if constexpr (g_leftAlignedAlu)
			{
				// the carry enters at the LSB of the 56-bit value, which is bit 8 of the register
				const RegScratch c(m_block);
				m_asm.set(asmjit::x86::CondCode::kC, c.get().r8());
				m_asm.movzx(r32(c.get()), c.get().r8());
				m_asm.shl(r64(c.get()), asmjit::Imm(8));
				m_asm.add(d, r64(c.get()));
			}
			else
			{
				m_asm.adc(d.get().r8(), asmjit::Imm(0));
			}

			m_asm.add(d, r64(s));
		}

		// C is set if bit 55 of the result is cleared
		m_asm.bt(d, asmjit::Imm(55 + g_aluBitOffset));
		ccr_update_ifNotCarry(CCRB_C);

		m_dspRegs.mask56(d);
	}

	void JitOps::op_Rep_Div(const TWord _op, const TWord _iterationCount)
	{
		m_blockRuntimeData.getEncodedInstructionCount() += _iterationCount;
		m_blockRuntimeData.getEncodedCycleCount() += (_iterationCount - 1) * dsp56k::calcCycles(Div, m_pcCurrentOp + 1, _op, m_block.dsp().memory().getBridgedMemoryAddress(), 1);

		const auto ab = getFieldValue<Div, Field_d>(_op);
		const auto jj = getFieldValue<Div, Field_JJ>(_op);

		m_ccrRead |= CCR_C;
		updateDirtyCCR(CCR_C);

		AluRef d(m_block, ab);

		const auto alu = d.get();

		auto ccrUpdateVL = [&]()
		{
			// V: Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
			// L: Set if the Overflow bit (V) is set.
			// What we do is we check if bits 55 and 54 of the ALU are not identical (host parity bit cleared) and set V accordingly.
			// TODO: L is only derived from the last step here, but the DSP makes it sticky by ORing the
			// per-step V across all N steps. Reproducing that needs the per-step V accumulated inside the
			// loop, which costs instructions in the hottest block in the emulator, so it is deliberately
			// not done. It only differs for a division whose dividend is out of range, see the last case
			// of rep_div_powerOfTwo.
			{
				const RegGP r(m_block);
				m_asm.ror(r, alu, 54 + g_aluBitOffset);
				m_asm.and_(r, asmjit::Imm(0x3));
			}
			ccr_vl_update_ifNotParity();
		};

		DspValue sPos(m_block, UsePooledTemp);

		RegGP carry(m_block);

		ShiftReg s(m_block);

		DspValue sNeg(m_block, true);
		sNeg.temp(DspValue::Temp56);

		decode_JJ_read(sPos, jj);

		// force S to be always positive

		// left shift by 24 and signextend to full 64 bit
		m_asm.shl(r64(sPos), asmjit::Imm(40));
		m_asm.sar(r64(sPos), asmjit::Imm(16 - g_aluBitOffset));	// land on the ALU field position

		// copy tosNeg and negate
		m_asm.mov(r64(sNeg), r64(sPos));
		m_asm.neg(r64(sNeg));

		// swap sNeg and sPos if needed
		m_asm.cmovns(r64(s), r64(sNeg));
		m_asm.cmovns(r64(sNeg), r64(sPos));
		m_asm.cmovns(r64(sPos), r64(s));

		// The loop carries its state in the host sign flag and lets the register wrap where the DSP wraps,
		// so it has to run in the LEFT-aligned domain: there the DSP's bit 55 is bit 63, which is the bit an
		// add already reports in SF, and the DSP's modulo 2^56 is the register's own modulo 2^64. Converting
		// to the right-aligned domain instead, which this used to do, reads bit 63 for a sign that lives at
		// bit 55 and never wraps - indistinguishable while the accumulator stays inside 56 bits, and wrong
		// for every step after one overflows. aarch64 always did it this way.
		aluSignextendTo64(alu);

		m_asm.copyBitToReg(carry, m_dspRegs.getSR(JitDspRegs::Read), CCRB_C);
		if constexpr (g_aluBitOffset)
			m_asm.shl(carry.get(), asmjit::Imm(g_aluBitOffset));	// the carry enters at the accumulator LSB

		const auto loopIteration = [&](const bool _needsTestAlu, const bool _updateCCR)
		{
			m_asm.mov(s, r64(sPos));
			if (_needsTestAlu)
				m_asm.test(alu, alu);
			m_asm.cmovns(s, r64(sNeg));
			m_asm.lea(alu, ptr(carry, alu, 1));
			m_asm.add(alu, s.get());

			// C is set if bit 55 of the result is cleared
			if (_updateCCR)
				ccr_update(CCRB_C, asmjit::x86::CondCode::kNotSign);
			else
				m_asm.setns(carry.get().r8());
		};

		// The sign of the previous ALU decides BOTH which value is added (+|s| or -|s|) and what the carry
		// into the LSB is (0 or 1), so a step adds one of two loop invariant values. Folding the carry into
		// sNeg turns a step into mov/cmov/add/add and drops the setns from the dependency chain. Only the
		// first step is different, its carry comes from SR rather than from a previous step.
		// lea because it does not touch the flags the next step needs.
		const auto foldCarryIntoSNeg = [&]()
		{
			m_asm.lea(r64(sNeg), ptr(r64(sNeg), static_cast<int32_t>(1 << g_aluBitOffset)));
		};

		const auto loopIterationInvariant = [&](const bool _needsTestAlu, const bool _updateCCR)
		{
			m_asm.mov(s, r64(sPos));
			if (_needsTestAlu)
				m_asm.test(alu, alu);
			m_asm.cmovns(s, r64(sNeg));
			m_asm.add(alu, alu);
			m_asm.add(alu, s.get());

			// C is set if bit 55 of the result is cleared
			if (_updateCCR)
				ccr_update(CCRB_C, asmjit::x86::CondCode::kNotSign);
		};

		// A rep/div whose divisor is a power of two and whose dividend is already in range is a shift,
		// not N dependent steps. Both are runtime properties, so the guard is emitted rather than assumed:
		// the divisor must be a power of two, and the dividend must satisfy 0 <= alu < divisor. One
		// unsigned compare covers both halves of the range test, because a negative accumulator is huge
		// when read unsigned, and it also rejects a zero divisor (which passes the power-of-two test).
		// In that domain N steps of non-restoring division produce, exactly,
		//     alu = ((alu << N) mod 2S) - S + (alu >> (log2(2S) - N)) + carry * 2^(N-1)
		// with V cleared and L untouched. Verified against the interpreter for every divisor 2^0..2^23,
		// every iteration count 1..24, both carry values, at every piecewise boundary of the domain.
		// TODO: only a power of two divisor is handled. The general case is a real division of
		// (alu << N) by 2S, which x64 could do with a single div rdx:rax while aarch64 would need a
		// different approach. Just 1 of the 8 rep/div sites in the Virus C ROM has a power of two
		// divisor and none of the other 7 show up in a profile, so measure before building it.
		const auto fastPathEnd = m_asm.newLabel();
		// Below a handful of iterations the loop is simply cheaper: a step is two cycles of latency,
		// while the closed form costs about seven regardless of N, so the crossover sits near four. No
		// shipped Virus ROM reps a div fewer than 7 times (TI/Snow use 7, 12, 16 and 24, B and C only 12
		// and 24), so the floor never rejects a real site - it only keeps the guard off code that could
		// not profit from it.
		const auto hasFastPath = _iterationCount >= 4 && _iterationCount <= 24;

		if (hasFastPath)
		{
			const auto slowPath = m_asm.newLabel();

			// s and sNeg are dead on the fast path. s is rcx, which is what the variable shift needs anyway.
			const auto q = r64(sNeg);
			const auto t = s.get();

			m_asm.lea(t, ptr(r64(sPos), static_cast<int32_t>(-1)));
			m_asm.test(t, r64(sPos));					// S & (S-1), zero if S is a power of two or zero
			m_asm.jnz(slowPath);
			m_asm.cmp(alu, r64(sPos));
			m_asm.jae(slowPath);						// unsigned, so this rejects alu < 0 and S == 0 too

			m_asm.bsf(t, r64(sPos));					// log2(S), well defined because S != 0 here
			if (_iterationCount > 1)
				m_asm.sub(t, asmjit::Imm(_iterationCount - 1));
			m_asm.mov(q, alu);
			m_asm.shr(q, t.r8());						// q = alu >> (log2(2S) - N), unscaled
			if constexpr (g_aluBitOffset)
				m_asm.shl(q, asmjit::Imm(g_aluBitOffset));

			m_asm.shl(alu, asmjit::Imm(_iterationCount));
			m_asm.lea(t, ptr(r64(sPos), r64(sPos), 0, static_cast<int32_t>(-1)));
			m_asm.and_(alu, t);							// (alu << N) mod 2S
			m_asm.sub(alu, r64(sPos));
			m_asm.add(alu, q);
			if (_iterationCount > 1)
				m_asm.shl(carry.get(), asmjit::Imm(_iterationCount - 1));
			m_asm.add(alu, carry.get());

			// C is set if bit 55 of the result is cleared. V is always cleared here and L stays untouched,
			// but both still have to go through the same helpers the slow path uses so that the compile time
			// CCR dirty/written state is identical on both paths.
			m_asm.test_(alu);
			ccr_update(CCRB_C, asmjit::x86::CondCode::kNotSign);
			ccr_clear(CCR_V);
			ccr_clearDirty(CCR_L);	// V is 0 here, so ORing it into L would only emit a no-op

			m_asm.jmp(fastPathEnd);
			m_asm.bind(slowPath);
		}

		// loop
		if (_iterationCount <= 24)
		{
			if(_iterationCount > 1)
			{
				loopIteration(true, false);
				foldCarryIntoSNeg();
				for(TWord i=1; i<_iterationCount-1; ++i)
					loopIterationInvariant(false, false);
			}
		}
		else
		{
			RegGP lc(m_block);
			m_asm.mov(r32(lc), _iterationCount - 1);

			const auto start = m_asm.newLabel();
			m_asm.bind(start);

			loopIteration(true, false);
			if constexpr (g_aluBitOffset)
				m_asm.shl(carry.get(), asmjit::Imm(g_aluBitOffset));	// setns gives a bare 1

			m_asm.dec(r32(lc));
			m_asm.jnz(start);

			foldCarryIntoSNeg();
		}

		// once
		ccrUpdateVL();
		if(_iterationCount > 1)
			loopIterationInvariant(true, true);
		else
			loopIteration(true, true);

		if (hasFastPath)
			m_asm.bind(fastPathEnd);

		m_dspRegs.mask56(alu);
	}

	void JitOps::op_Not(TWord op)
	{
		const auto ab = getFieldValue<Not, Field_d>(op);

		{
			DspValue d(m_block);
			getALU1(d, ab);
			m_asm.not_(r32(d));
			m_asm.and_(r32(d), asmjit::Imm(0xffffff));
			setALU1(ab, d);

			copyBitToCCR(d.get(), 23, CCRB_N);

			m_asm.test_(d.get());
			ccr_update_ifZero(CCRB_Z);					// Set if bits 47�24 of the result are 0
		}

		ccr_clear(CCR_V);								// Always cleared
	}

	void JitOps::op_Rol(TWord op)
	{
		const auto D = getFieldValue<Rol, Field_d>(op);

		DspValue r(m_block);
		getALU1(r, D);

		const RegGP prevCarry(m_block);
		m_asm.clr(prevCarry);

		ccr_getBitValue(prevCarry, CCRB_C);

		copyBitToCCR(r.get(), 23, CCRB_C);						// Set if bit 47 of the destination operand is set, and cleared otherwise

		m_asm.shl(r.get(), asmjit::Imm(1));
		ccr_n_update_by23(r64(r));								// Set if bit 47 of the result is set

		m_asm.or_(r.get(), r32(prevCarry));						// Set if bits 47�24 of the result are 0
		ccr_update_ifZero(CCRB_Z);
		setALU1(D, r);

		ccr_clear(CCR_V);										// This bit is always cleared
	}

	void JitOps::op_Ror(TWord op)
	{
		const auto D = getFieldValue<Ror, Field_d>(op);

		DspValue r(m_block);
		getALU1(r, D);

		const RegGP prevCarry(m_block);
		m_asm.clr(prevCarry);

		ccr_getBitValue(prevCarry, CCRB_C);

		copyBitToCCR(r.get(), 0, CCRB_C);						// C = bit 24 of the destination

		m_asm.shr(r.get(), asmjit::Imm(1));
		m_asm.shl(r32(prevCarry), asmjit::Imm(23));
		m_asm.or_(r.get(), r32(prevCarry));						// inject old carry into bit 47 position

		ccr_n_update_by23(r64(r));								// Set if bit 47 of the result is set
		ccr_update_ifZero(CCRB_Z);								// Set if bits 47-24 of the result are 0
		setALU1(D, r);

		ccr_clear(CCR_V);										// This bit is always cleared
	}

	void JitOps::op_Subl(TWord op)
	{
		const auto ab = getFieldValue<Subl, Field_d>(op);

		AluRef d(m_block, ab ? 1 : 0, true, true);

		const RegGP oldBit55(m_block);
		m_asm.copyBitToReg(oldBit55, d, 55 + g_aluBitOffset);

		aluSignextendTo64(d);
		m_asm.shl(d, asmjit::Imm(1));

		{
			const RegGP s(m_block);
			aluSignextendTo64(s, r64(m_dspRegs.getALU(ab ? 0 : 1)));

			m_asm.sub(d, s);
		}

		ccr_dirty(ab ? 1 : 0, d, static_cast<CCRMask>(CCR_E | CCR_U | CCR_N | CCR_Z));

		const RegGP newBit55(m_block);
		m_asm.copyBitToReg(newBit55, d, 55 + g_aluBitOffset);

		m_asm.xor_(oldBit55.get().r8(), newBit55.get().r8());
		copyBitToCCR(oldBit55, 0, CCRB_V);
		// Carry bit note: "The Carry bit (C) is set correctly if the source operand does not overflow as a result of the left shift operation.", we do not care at the moment
		copyBitToCCR(oldBit55, 0, CCRB_C);

		m_dspRegs.mask56(d);
	}
}

#endif
