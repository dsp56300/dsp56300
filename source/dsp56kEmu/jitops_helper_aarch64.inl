#pragma once

#include "jitops.h"

namespace dsp56k
{
	inline void JitOps::updateAddressRegister(const JitReg32& _r, const JitReg32& _n, const JitReg32& _m, uint32_t _rrr)
	{
		const auto linear = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		const AguRegMmask moduloMask(m_block, _rrr);

		m_asm.mov(r32(regReturnVal), asmjit::Imm(0xffffff));	// linear shortcut
		m_asm.cmp(r32(_m), r32(regReturnVal));
		m_asm.jz(linear);

		m_asm.tst(_m, asmjit::Imm(0xffff));						// bit reverse
		m_asm.cond_zero().b(bitreverse);

		m_asm.and_(r32(regReturnVal), _m, asmjit::Imm(0xffff));
		m_asm.cmp(r32(regReturnVal), asmjit::Imm(0x8000));
		m_asm.cond_ge().b(multipleWrapModulo);

		const auto nAbs = r32(regReturnVal);					// compare abs(n) with m
		m_asm.cmp(r32(_n), asmjit::Imm(0));
		m_asm.cneg(nAbs, r32(_n), asmjit::arm::CondCode::kLT);

		m_asm.cmp(nAbs, _m);									// modulo or linear
		m_asm.cond_gt().b(linear);

		// modulo:
		m_asm.bind(modulo);
		updateAddressRegisterModulo(r32(_r), _n, r32(_m), r32(moduloMask));
		m_asm.jmp(end);

		// multiple-wrap modulo:
		m_asm.bind(multipleWrapModulo);
		updateAddressRegisterMultipleWrapModulo(_r, _n, _m);
		m_asm.jmp(end);

		// bitreverse:
		m_asm.bind(bitreverse);
		updateAddressRegisterBitreverse(_r, _n, _m);
		m_asm.jmp(end);

		// linear:
		m_asm.bind(linear);
		m_asm.add(_r, _n);

		m_asm.bind(end);
		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	inline void JitOps::updateAddressRegisterConst(const JitReg32& _r, const int _n, const JitReg32& _m, uint32_t _rrr)
	{
		const auto linear = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		const AguRegMmask moduloMask(m_block, _rrr, true);

		m_asm.mov(r32(regReturnVal), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.cmp(r32(_m), r32(regReturnVal));
		m_asm.jz(linear);

		m_asm.tst(_m, asmjit::Imm(0xffff));							// bit reverse
		m_asm.cond_zero().b(end);

		m_asm.and_(r32(regReturnVal), _m, asmjit::Imm(0xffff));		// multiple-wrap modulo
		m_asm.cmp(r32(regReturnVal), asmjit::Imm(0x8000));
		m_asm.cond_ge().b(end);

		// modulo:
		m_asm.bind(modulo);
		{
			const ShiftReg shifter(m_block);
			const auto& p64 = shifter;
			const auto p = r32(p64.get());

			m_asm.mov(p, _r);
			m_asm.and_(p, r32(moduloMask));
			const auto& modulo = _m;	// and modulo is m+1
			if (_n == -1)
			{
				m_asm.dec(_r);
				m_asm.dec(p);

				m_asm.sar(p, asmjit::Imm(31));
				m_asm.inc(modulo);
				m_asm.and_(p, modulo);
				m_asm.dec(modulo);
				m_asm.add(_r, p);
			}
			else	// _n==1
			{
				m_asm.inc(_r);		// Increment r by n here.
				m_asm.inc(p);

				const auto& mtMinusP64 = regReturnVal;
				const auto mtMinusP = r32(mtMinusP64);

				m_asm.mov(mtMinusP, _m);
				m_asm.sub(mtMinusP, p);
				m_asm.sar(mtMinusP, asmjit::Imm(31));
				m_asm.inc(modulo);
				m_asm.and_(mtMinusP, modulo);
				m_asm.dec(modulo);
				m_asm.sub(_r, mtMinusP);
			}

		}
		m_asm.jmp(end);

		// linear:
		m_asm.bind(linear);

		if (_n == 1)
			m_asm.inc(_r);
		else if (_n == -1)
			m_asm.dec(_r);
		else
			m_asm.add(_r, _n);

		m_asm.bind(end);
		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	void JitOps::setALU0(const uint32_t _aluIndex, const JitRegGP& _src)
	{
		AluRef d(m_block, _aluIndex, true, true);
		m_asm.bfi(d, r64(_src), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitOps::setALU1(const uint32_t _aluIndex, const JitReg32& _src)
	{
		AluRef d(m_block, _aluIndex, true, true);
		m_asm.bfi(d, r64(_src), asmjit::Imm(24), asmjit::Imm(24));
	}

	void JitOps::setALU2(const uint32_t _aluIndex, const JitReg32& _src)
	{
		AluRef d(m_block, _aluIndex, true, true);
		m_asm.bfi(d, r64(_src), asmjit::Imm(48), asmjit::Imm(8));
	}

	void JitOps::setSSH(const JitReg32& _src) const
	{
		incSP();
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			m_asm.bfi(_ss, r64(_src), asmjit::Imm(24), asmjit::Imm(24));
		}, true, true);
	}

	void JitOps::setSSL(const JitReg32& _src) const
	{
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			m_asm.bfi(_ss, r64(_src), asmjit::Imm(0), asmjit::Imm(24));
		}, true, true);
	}

	inline void JitOps::setSSHSSL(const JitReg32& _ssh, const JitReg32& _ssl)
	{
		incSP();
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			m_asm.bfi(_ss, r64(_ssh), asmjit::Imm(24), asmjit::Imm(24));
			m_asm.bfi(_ss, r64(_ssl), asmjit::Imm(0), asmjit::Imm(24));
		}, false, true);
	}

	void JitOps::setMR(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_asm.mov(r, m_dspRegs.getSR(JitDspRegs::Read));
		m_asm.bfi(r, _src, asmjit::Imm(8), asmjit::Imm(8));
		m_asm.mov(m_dspRegs.getSR(JitDspRegs::Write), r.get());
	}

	void JitOps::setEOM(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().omr);
		m_asm.bfi(r, _src, asmjit::Imm(8), asmjit::Imm(8));
		m_block.dspRegPool().movDspReg(m_block.dsp().regs().omr, r);
	}

	void JitOps::decSP() const
	{
		const RegGP r(m_block);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().sp);		m_asm.dec(r);		m_block.dspRegPool().movDspReg(m_block.dsp().regs().sp, r);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().sc);		m_asm.dec(r);		m_block.dspRegPool().movDspReg(m_block.dsp().regs().sc, r);
	}

	void JitOps::incSP() const
	{
		const RegGP r(m_block);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().sp);		m_asm.inc(r);		m_block.dspRegPool().movDspReg(m_block.dsp().regs().sp, r);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().sc);		m_asm.inc(r);		m_block.dspRegPool().movDspReg(m_block.dsp().regs().sc, r);
	}

	void JitOps::transferSaturation(const JitRegGP& _dst)
	{
		// scaling

		/*
		if( sr_test_noCache(SR_S1) )
			_scale.var <<= 1;
		else if( sr_test_noCache(SR_S0) )
			_scale.var >>= 1;
		*/

		{
			const ShiftReg shifter(m_block);
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S1);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.lsl(_dst, _dst, shifter.get());

			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S0);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.lsr(_dst, _dst, shifter.get());
		}

		// saturated transfer
		/*
		const int64_t& test = _src.signextend<int64_t>();

		if( test < -140737488355328 )			// ff ff 800000 000000
		{
			sr_set( SR_L );
			_dst = 0x800000;
		}
		else if( test > 140737471578112 )		// 00 00 7fffff 000000
		{
			sr_set( SR_L );
			_dst = 0x7FFFFF;
		}
		else
			_dst = static_cast<int>(_src.var >> 24) & 0xffffff;
		*/
		{
			const RegGP tester(m_block);
			m_asm.mov(tester, _dst);
			signextend56to64(tester);

			// non-limited default
			m_asm.shr(_dst, asmjit::Imm(24));
			m_asm.and_(_dst, asmjit::Imm(0x00ffffff));

			// lower limit
			{
				const auto limit = regReturnVal;
				m_asm.mov(limit, asmjit::Imm(0xffff800000000000));
				m_asm.cmp(tester, limit);
			}
			{
				const auto minmax = regReturnVal;
				m_asm.mov(minmax, 0x800000);
				m_asm.csel(_dst, minmax, _dst, asmjit::arm::CondCode::kLT);
			}
			ccr_update_ifLess(CCRB_L);

			// upper limit
			{
				const auto limit = regReturnVal;
				m_asm.mov(limit, asmjit::Imm(0x00007fffff000000));
				m_asm.cmp(tester, limit);
			}
			{
				const auto minmax = regReturnVal;
				m_asm.mov(minmax, 0x7fffff);
				m_asm.csel(_dst, minmax, _dst, asmjit::arm::CondCode::kGT);
			}
		}
		ccr_update_ifGreater(CCRB_L);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_bbbbb>()>::type*> void JitOps::bitTest(TWord op, const JitRegGP& _value, const ExpectedBitValue _bitValue, const asmjit::Label _skip) const
	{
		const auto bit = getBit<Inst>(op);
		m_asm.bitTest(_value, bit);

		if (_bitValue == BitSet)
			m_asm.cond_zero().b(_skip);
		else if (_bitValue == BitClear)
			m_asm.cond_not_zero().b(_skip);
	}
}
