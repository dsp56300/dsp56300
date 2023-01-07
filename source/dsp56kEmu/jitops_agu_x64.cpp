#include "jittypes.h"

#ifdef HAVE_X86_64

#include "jitops.h"
#include "jitconfig.h"

namespace dsp56k
{
	void JitOps::updateAddressRegisterSub(const JitReg32& _r, const JitReg32& _n, const JitReg32& _m, uint32_t _rrr, bool _addN)
	{
		const auto mode = m_block.getAddressingMode(_rrr);

		if(mode != AddressingMode::Unknown)
		{
			updateAddressRegisterSub(mode, _r, _n, _m, _rrr, _addN);
			return;
		}

		const auto notLinear = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();
		
		const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jnz(notLinear);

		// linear:
		if (_addN)
			m_asm.add(_r, _n);
		else
			m_asm.sub(_r, _n);

		m_asm.jmp(end);

		m_asm.bind(notLinear);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			m_asm.test_(_m.r16());							// bit reverse
			m_asm.jz(bitreverse);
		}

		m_asm.cmp(_m.r16(), asmjit::Imm(0x7fff));			// multi-wrap modulo
		m_asm.ja(multipleWrapModulo);

		// modulo:
		updateAddressRegisterSubModulo(r32(_r), _n, r32(_m), r32(moduloMask), _addN);
		m_asm.jmp(end);

		// multiple-wrap modulo:
		m_asm.bind(multipleWrapModulo);
		if(m_block.getConfig().aguSupportMultipleWrapModulo)
			updateAddressRegisterSubMultipleWrapModulo(_r, _n, r32(moduloMask), _addN);

		if(m_block.getConfig().aguSupportBitreverse)
			m_asm.jmp(end);

		// bitreverse:
		m_asm.bind(bitreverse);
		updateAddressRegisterSubBitreverse(_r, _n, _addN);

		m_asm.bind(end);
		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	void JitOps::updateAddressRegisterSubN1(const JitReg32& _r, const JitReg32& _m, uint32_t _rrr, bool _addN)
	{
		const auto mode = m_block.getAddressingMode(_rrr);
		if(mode != AddressingMode::Unknown)
		{
			updateAddressRegisterSubN1(mode, _r, _m, _rrr, _addN);
			return;
		}

		const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);
		const auto notLinear = m_asm.newLabel();
		const auto end = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto multiwrapModulo = m_asm.newLabel();

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jnz(notLinear);

		if (_addN)	m_asm.inc(_r);
		else		m_asm.dec(_r);

		m_asm.jmp(end);

		m_asm.bind(notLinear);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			m_asm.test_(_m.r16());							// bit reverse
			m_asm.jz(bitreverse);
		}

		m_asm.cmp(_m.r16(), asmjit::Imm(0x7fff));			// multi-wrap modulo
		m_asm.ja(multiwrapModulo);

		// modulo:
		updateAddressRegisterSubModuloN1(_r, _m, r32(moduloMask), _addN);
		m_asm.jmp(end);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			// bitreverse
			m_asm.bind(bitreverse);
			updateAddressRegisterSubBitreverseN1(_r, _addN);

			if(m_block.getConfig().aguSupportMultipleWrapModulo)
				m_asm.jmp(end);
		}

		m_asm.bind(multiwrapModulo);
		if(m_block.getConfig().aguSupportMultipleWrapModulo)
			updateAddressRegisterSubMultipleWrapModuloN1(_r, _addN, r32(moduloMask));

		m_asm.bind(end);
		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	void JitOps::updateAddressRegisterSubModuloN1(const JitReg32& _r, const JitReg32& _m, const JitReg32& _mMask, bool _addN) const
	{
		const RegScratch scratch(m_block);
		const auto p = r32(scratch);

		if (!_addN)
		{
			m_asm.lea(p, ptr(_r, -1));
			m_asm.test(_r, _mMask);
			m_asm.lea(_r, ptr(_r, _m));
			m_asm.cmovne(_r, p);
		}
		else
		{
			m_asm.mov(p, _r);
			m_asm.and_(p, _mMask);
			m_asm.cmp(p, _m);
			m_asm.mov(p, -1);
			m_asm.cmove(p, _m);
			m_asm.sub(_r, p);
		}
	}

	void JitOps::updateAddressRegisterSubModulo(const JitReg32& r, const JitReg32& n, const JitReg32& m, const JitReg32& mMask, bool _addN) const
	{
		const ShiftReg shift(m_block);
		const RegScratch scratch(m_block);

		const auto lowerBound = r32(shift.get());
		const auto mod = r32(scratch);

		signextend24To32(n);

		if(m_asm.hasBMI())
		{
			m_asm.andn(lowerBound, mMask, r);
		}
		else
		{
			m_asm.mov(lowerBound, mMask);
			m_asm.not_(lowerBound);
			m_asm.and_(lowerBound, r);
		}

		if(_addN)
			m_asm.add(r, n);
		else
			m_asm.sub(r, n);

		m_asm.mov(mod, m);
		m_asm.inc(mod);

		// if (n & mask) == 0
		//     mod = 0
		m_asm.and_(n, mMask);
		m_asm.cmovz(mod, n);

		// if (r < lowerbound)
		//     r += mod
		m_asm.lea(n, ptr(r, mod));
		m_asm.cmp(r, lowerBound);
		m_asm.cmovl(r, n);

		// if r > upperBound
		//     r -= mod
		m_asm.add(lowerBound, m);
		m_asm.mov(n, r);
		m_asm.sub(n, mod);
		m_asm.cmp(r, lowerBound);
		m_asm.cmovg(r, n);
	}
}
#endif