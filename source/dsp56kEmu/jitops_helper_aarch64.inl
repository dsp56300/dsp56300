#pragma once

#include "jitops.h"

namespace dsp56k
{
	inline void JitOps::updateAddressRegister(const JitReg32& _r, const JitReg32& _n, const JitReg32& _m)
	{
		const auto linear = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jz(linear);

		m_asm.tst(_m, asmjit::Imm(0xffff));				// bit reverse
		m_asm.cond_zero().b(bitreverse);

		m_asm.and_(regReturnVal, _m, asmjit::Imm(0xffff));
		m_asm.cmp(regReturnVal, asmjit::Imm(0x7fff));
		m_asm.cond_gt().b(multipleWrapModulo);

		const auto nAbs = r32(regReturnVal);			// compare abs(n) with m
		m_asm.cmp(r32(_n), asmjit::Imm(0));
		m_asm.cneg(nAbs, r32(_n), asmjit::arm::Cond::kLT);

		m_asm.cmp(nAbs, _m);							// modulo or linear
		m_asm.cond_gt().b(linear);

		// modulo:
		m_asm.bind(modulo);
		updateAddressRegisterModulo(r32(_r), _n, r32(_m));
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

	inline void JitOps::updateAddressRegisterConst(const JitReg32& _r, const int _n, const JitReg32& _m)
	{
		const auto linear = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));			// linear shortcut
		m_asm.jz(linear);

		m_asm.tst(_m, asmjit::Imm(0xffff));					// bit reverse
		m_asm.cond_zero().b(end);

		m_asm.and_(regReturnVal, _m, asmjit::Imm(0xffff));	// multiple-wrap modulo
		m_asm.cmp(regReturnVal, asmjit::Imm(0x7fff));
		m_asm.cond_gt().b(end);

		// modulo:
		m_asm.bind(modulo);
		{
			const auto moduloMask = regReturnVal;
			const ShiftReg shifter(m_block);
			const auto& p64 = shifter;
			const auto p = r32(p64.get());

			m_asm.bsr(shifter, _m);								// returns index of MSB that is 1
			m_asm.mov(moduloMask, asmjit::Imm(2));
			m_asm.shl(moduloMask, shifter.get());
			m_asm.dec(moduloMask);

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

				const auto& mtMinusP64 = moduloMask;
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
		m_asm.bfi(d, _src, asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitOps::setALU1(const uint32_t _aluIndex, const JitReg32& _src)
	{
		AluRef d(m_block, _aluIndex, true, true);
		m_asm.bfi(d, _src, asmjit::Imm(24), asmjit::Imm(24));
	}

	void JitOps::setALU2(const uint32_t _aluIndex, const JitReg32& _src)
	{
		AluRef d(m_block, _aluIndex, true, true);
		m_asm.bfi(d, _src, asmjit::Imm(48), asmjit::Imm(8));
	}
}
