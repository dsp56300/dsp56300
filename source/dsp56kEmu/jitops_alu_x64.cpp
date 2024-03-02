#include "jittypes.h"

#ifdef HAVE_X86_64

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
		m_asm.shl(r64(_v), asmjit::Imm(24));

		AluRef alu(m_block, ab);

		m_asm.test(alu, r64(_v));
		ccr_update_ifZero(CCRB_Z);

		{
			const RegScratch mask(m_block);
			m_asm.mov(mask, asmjit::Imm(0xff000000ffffff));
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

		// we want to hit the 64 bit boundary to make use of the native carry flag so we pre-shift by 8 bit (56 => 64)
		m_asm.sal(alu, asmjit::Imm(8));

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
		
		m_asm.shr(alu, asmjit::Imm(8));				// correction for our pre-shift by 8

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg* _v, TWord _immediate/* = 0*/)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_dspRegs.getALU(alu, _abSrc);

		CcrBatchUpdate bu(*this, CCR_C, CCR_V);

		m_asm.sal(alu, asmjit::Imm(8));
		if(_v)
			m_asm.sar(alu, _v->get().r8());
		else
			m_asm.sar(alu, asmjit::Imm(_immediate));
		m_asm.shr(alu, asmjit::Imm(8));

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
			signextend56to64(d);

		const JitDspMode* mode = m_block.getMode();

//		JitDspMode testMode;
//		testMode.initialize(m_block.dsp());
//		assert(!mode || *mode == testMode);

		if(mode)
		{
			uint64_t rounder = 0x800000;

			if(mode->testSR(SRB_S1))	rounder >>= 1;
			if(mode->testSR(SRB_S0))	rounder <<= 1;

			m_asm.add(d, asmjit::Imm(rounder));
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
					m_asm.mov(aluIfAndWithMaskIsZero, d);
					m_asm.and_(aluIfAndWithMaskIsZero, asmjit::Imm(rounder));
					m_asm.test(d, asmjit::Imm(mask));
					m_asm.cmovz(d, aluIfAndWithMaskIsZero.get());
				}
			}
			
			// all bits to the right of and including the rounding position are cleared.
			// _alu.var&=~mask;
			mask = ~mask;
			m_asm.and_(d, asmjit::Imm(mask));
		}
		else
		{
			const RegGP rounder(m_block);

			m_asm.mov(r32(rounder), asmjit::Imm(0x800000));

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
			m_asm.ror(r, d.get(), 54);
			m_asm.and_(r, asmjit::Imm(0x3));
		}

		ccr_update_ifNotParity(CCRB_V);
		ccr_l_update_by_v();

		{
			DspValue s(m_block);
			decode_JJ_read(s, jj);

			m_asm.shl(r64(s), asmjit::Imm(40));
			m_asm.sar(r64(s), asmjit::Imm(16));

			const RegGP addOrSub(m_block);
			m_asm.mov(addOrSub, r64(s));
			m_asm.xor_(addOrSub, d.get());

			{
				const RegScratch sNeg(m_block);
				m_asm.mov(sNeg, r64(s));
				m_asm.neg(sNeg);

				m_asm.bt(addOrSub, asmjit::Imm(55));

				m_asm.cmovnc(r64(s), sNeg);
			}

			m_asm.add(d, d);

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(CCRB_C));
			m_asm.adc(d.get().r8(), asmjit::Imm(0));

			m_asm.add(d, r64(s));
		}

		// C is set if bit 55 of the result is cleared
		m_asm.bt(d, asmjit::Imm(55));
		ccr_update_ifNotCarry(CCRB_C);

		m_dspRegs.mask56(d);
	}

	void JitOps::op_Rep_Div(const TWord _op, const TWord _iterationCount)
	{
		m_blockRuntimeData.getEncodedInstructionCount() += _iterationCount;

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
			{
				const RegGP r(m_block);
				m_asm.ror(r, alu, 54);
				m_asm.and_(r, asmjit::Imm(0x3));
			}
			ccr_update_ifNotParity(CCRB_V);
			ccr_l_update_by_v();
		};

		DspValue s(m_block, UsePooledTemp);

		decode_JJ_read(s, jj);

		RegGP addOrSub(m_block);
		RegGP carry(m_block);
		ShiftReg sNeg(m_block);

		const auto loopIteration = [&](bool last)
		{
			m_asm.mov(addOrSub, r64(s));
			m_asm.xor_(addOrSub, alu);

			m_asm.mov(sNeg, r64(s));
			m_asm.neg(sNeg);
			m_asm.bt(addOrSub, asmjit::Imm(55));
			m_asm.cmovc(sNeg, r64(s));

//			m_asm.add(alu, alu);
//			m_asm.add(alu, carry.get());
			m_asm.lea(alu, asmjit::x86::ptr(carry, alu, 1));
			m_asm.add(alu, sNeg.get());

			// C is set if bit 55 of the result is cleared
			m_asm.bt(alu, asmjit::Imm(55));
			if (last)
				ccr_update_ifNotCarry(CCRB_C);
			else
				m_asm.setnc(carry.get().r8());
		};

		// once
		m_asm.shl(r64(s), asmjit::Imm(40));
		m_asm.sar(r64(s), asmjit::Imm(16));

		m_asm.copyBitToReg(carry, m_dspRegs.getSR(JitDspRegs::Read), CCRB_C);

		// loop
		{
			RegGP lc(m_block);
			m_asm.mov(r32(lc), _iterationCount - 1);

			const auto start = m_asm.newLabel();
			m_asm.bind(start);

			loopIteration(false);

			m_asm.dec(r32(lc));
			m_asm.jnz(start);
		}

		// once
		ccrUpdateVL();
		loopIteration(true);

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
			ccr_update_ifZero(CCRB_Z);					// Set if bits 47–24 of the result are 0
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

		m_asm.or_(r.get(), r32(prevCarry));						// Set if bits 47–24 of the result are 0
		ccr_update_ifZero(CCRB_Z);
		setALU1(D, r);

		ccr_clear(CCR_V);										// This bit is always cleared
	}

	void JitOps::op_Subl(TWord op)
	{
		const auto ab = getFieldValue<Subl, Field_d>(op);

		AluRef d(m_block, ab ? 1 : 0, true, true);

		const RegGP oldBit55(m_block);
		m_asm.copyBitToReg(oldBit55, d, 55);

		signextend56to64(d);
		m_asm.shl(d, asmjit::Imm(1));

		RegGP s(m_block);
		signextend56to64(s, r64(m_dspRegs.getALU(ab ? 0 : 1)));

		m_asm.sub(d, s);
		s.release();

		ccr_dirty(ab ? 1 : 0, d, static_cast<CCRMask>(CCR_E | CCR_U | CCR_N | CCR_Z));

		const RegGP newBit55(m_block);
		m_asm.copyBitToReg(newBit55, d, 55);

		m_asm.xor_(oldBit55.get().r8(), newBit55.get().r8());
		copyBitToCCR(oldBit55, 0, CCRB_V);
		// Carry bit note: "The Carry bit (C) is set correctly if the source operand does not overflow as a result of the left shift operation.", we do not care at the moment
		copyBitToCCR(oldBit55, 0, CCRB_C);

		m_dspRegs.mask56(d);
	}
}

#endif
