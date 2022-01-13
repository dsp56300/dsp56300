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

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jz(linear);

		m_asm.or_(_m.r16(), _m.r16());					// bit reverse
		m_asm.jz(bitreverse);

		m_asm.cmp(_m.r16(), asmjit::Imm(0x7fff));
		m_asm.jg(multipleWrapModulo);

		const auto nAbs = r32(regReturnVal);			// compare abs(n) with m
		m_asm.mov(nAbs, _n);
		m_asm.neg(nAbs);
		m_asm.cmovl(nAbs, _n);

		m_asm.cmp(nAbs, _m);							// modulo or linear
		m_asm.jg(linear);

		// modulo:
		m_asm.bind(modulo);
		updateAddressRegisterModulo(r32(_r), _n, r32(_m), moduloMask.r32());
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
		const auto notLinear = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		const AguRegMmask moduloMask(m_block, _rrr, true);

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jnz(notLinear);

		if (_n == 1)
			m_asm.inc(_r);
		else if (_n == -1)
			m_asm.dec(_r);
		else
			m_asm.add(_r, _n);

		m_asm.jmp(end);

		m_asm.bind(notLinear);

		m_asm.or_(_m.r16(), _m.r16());					// bit reverse
		m_asm.jz(end);

		m_asm.cmp(_m.r16(), asmjit::Imm(0x7fff));
		m_asm.jg(end);

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

		m_asm.bind(end);
		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	void JitOps::setALU0(const uint32_t _aluIndex, const JitRegGP& _src)
	{
		const RegGP maskedSource(m_block);
		m_asm.mov(maskedSource, _src.r64());
		m_asm.and_(maskedSource, asmjit::Imm(0xffffff));

		AluRef temp(m_block, _aluIndex, true, true);

		m_asm.and_(temp.get(), asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp.get(), maskedSource.get());
	}

	void JitOps::setALU1(const uint32_t _aluIndex, const JitReg32& _src)
	{
		const RegGP maskedSource(m_block);
		m_asm.mov(maskedSource, r64(_src));
		m_asm.and_(maskedSource, asmjit::Imm(0xffffff));

		AluRef temp(m_block, _aluIndex, true, true);;

		m_asm.ror(temp, asmjit::Imm(24));
		m_asm.and_(temp, asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp.get(), maskedSource.get());
		m_asm.rol(temp, asmjit::Imm(24));
	}

	void JitOps::setALU2(const uint32_t _aluIndex, const JitReg32& _src)
	{
		const RegGP maskedSource(m_block);
		m_asm.mov(maskedSource, r64(_src));
		m_asm.and_(maskedSource, asmjit::Imm(0xff));

		AluRef temp(m_block, _aluIndex);

		m_asm.ror(temp, asmjit::Imm(48));
		m_asm.and_(temp.get(), asmjit::Imm(0xffffffffffffff00));
		m_asm.or_(temp.get(), maskedSource.get());
		m_asm.rol(temp, asmjit::Imm(48));
	}

	void JitOps::setSSH(const JitReg32& _src) const
	{
		incSP();
		const RegGP temp(m_block);
		m_dspRegs.getSS(temp);
		m_asm.ror(temp, asmjit::Imm(24));
		m_asm.and_(temp, asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp, r64(_src));
		m_asm.rol(temp, asmjit::Imm(24));
		m_dspRegs.setSS(temp);
	}

	void JitOps::setSSL(const JitReg32& _src) const
	{
		const RegGP temp(m_block);
		m_dspRegs.getSS(temp);
		m_asm.and_(temp, asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp, r64(_src));
		m_dspRegs.setSS(temp);
	}

	void JitOps::setMR(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_asm.mov(r, m_dspRegs.getSR(JitDspRegs::Read));
		m_asm.and_(r, asmjit::Imm(0xff00ff));
		m_asm.shl(_src, asmjit::Imm(8));
		m_asm.or_(r, _src);
		m_asm.shr(_src, asmjit::Imm(8));	// TODO: we don't wanna be destructive to the input for now
		m_asm.mov(m_dspRegs.getSR(JitDspRegs::Write), r.get());
	}

	void JitOps::setEOM(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xff00ff));
		m_asm.shl(_src, asmjit::Imm(8));
		m_asm.or_(r, _src);
		m_block.dspRegPool().movDspReg(m_block.dsp().regs().omr, r);
		m_asm.shr(_src, asmjit::Imm(8));	// TODO: we don't wanna be destructive to the input for now
	}

	void JitOps::decSP() const
	{
		m_asm.dec(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_block.dsp().regs().sp.var)));
		m_asm.dec(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_block.dsp().regs().sc.var)));
	}

	void JitOps::incSP() const
	{
		m_asm.inc(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_block.dsp().regs().sp.var)));
		m_asm.inc(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_block.dsp().regs().sc.var)));
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
			const ShiftReg s0s1(m_block);
			m_asm.xor_(s0s1, s0s1.get());

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_S1));
			m_asm.setc(s0s1.get().r8());
			m_asm.shl(_dst, s0s1.get().r8());

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_S0));
			m_asm.setc(s0s1.get().r8());
			m_asm.shr(_dst, s0s1.get().r8());
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
				m_asm.cmovl(_dst, minmax);
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
				m_asm.cmovg(_dst, minmax);
			}
		}
		ccr_update_ifGreater(CCRB_L);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_bbbbb>()>::type*> void JitOps::bitTest(TWord op, const JitRegGP& _value, const ExpectedBitValue _bitValue, const asmjit::Label _skip) const
	{
		const auto bit = getBit<Inst>(op);
		m_asm.bt(_value, asmjit::Imm(bit));

		if (_bitValue == BitSet)
			m_asm.jnc(_skip);
		else if (_bitValue == BitClear)
			m_asm.jc(_skip);
	}
}
