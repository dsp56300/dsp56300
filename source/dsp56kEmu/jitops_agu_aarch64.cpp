#include "jittypes.h"

#ifdef HAVE_ARM64

#include "jitops.h"
#include "jitconfig.h"

namespace dsp56k
{
	void JitOps::updateAddressRegisterSub(const JitReg32& _r, const JitReg32& _n, uint32_t _rrr, bool _addN)
	{
		const auto mode = m_block.getAddressingMode(_rrr);

		if(mode != AddressingMode::Unknown)
		{
			updateAddressRegisterSub(mode, _r, _n, _rrr, _addN);
			return;
		}

		const auto notLinear = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		const DspValue m = makeDspValueAguReg(m_block, PoolReg::DspM0, _rrr, true, false);
		const DspValue moduloMask = makeDspValueAguReg(m_block, PoolReg::DspM0mask, _rrr);

		{
			 // linear shortcut
			const RegScratch scratch(m_block);
			m_asm.add(r32(scratch), r32(m), asmjit::Imm(1));
			m_asm.tst(r32(scratch), asmjit::Imm(0xffff));
			m_asm.jnz(notLinear);
		}

		// linear:
		if (_addN)
			m_asm.add(_r, _n);
		else
			m_asm.sub(_r, _n);
		m_asm.jmp(end);

		m_asm.bind(notLinear);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			m_asm.tst(r32(m), asmjit::Imm(0xffff));						// bit reverse
			m_asm.jz(bitreverse);
		}

		{
			const RegScratch scratch(m_block);
			m_asm.and_(r32(scratch), r32(m), asmjit::Imm(0xffff));
			m_asm.cmp(r32(scratch), asmjit::Imm(0x8000));
		}
		m_asm.b(asmjit::arm::CondCode::kUnsignedGE, multipleWrapModulo);

		// modulo:
		updateAddressRegisterSubModulo(r32(_r), _n, r32(m), r32(moduloMask), _addN);
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
		maskSC1624(_r);
	}

	void JitOps::updateAddressRegisterSubN1(const JitReg32& _r, uint32_t _rrr, bool _addN)
	{
		const auto mode = m_block.getAddressingMode(_rrr);
		if(mode != AddressingMode::Unknown)
		{
			updateAddressRegisterSubN1(mode, _r, _rrr, _addN);
			return;
		}

		const DspValue moduloMask = makeDspValueAguReg(m_block, PoolReg::DspM0mask, _rrr);
		const DspValue m = makeDspValueAguReg(m_block, PoolReg::DspM0, _rrr, true, false);

		const auto notLinear = m_asm.newLabel();
		const auto end = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto multiwrapModulo = m_asm.newLabel();

		{
			const RegScratch scratch(m_block);
			m_asm.add(r32(scratch), r32(m), asmjit::Imm(1));				// linear shortcut
			m_asm.tst(r32(scratch), asmjit::Imm(0xffff));
			m_asm.jnz(notLinear);
		}

		if (_addN)	m_asm.inc(_r);
		else		m_asm.dec(_r);

		m_asm.jmp(end);

		m_asm.bind(notLinear);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			m_asm.tst(r32(m), asmjit::Imm(0xffff));							// bit reverse
			m_asm.jz(bitreverse);
		}

		{
			const RegScratch scratch(m_block);
			m_asm.and_(r32(scratch), r32(m), asmjit::Imm(0xffff));		// multiple-wrap modulo
			m_asm.cmp(r32(scratch), asmjit::Imm(0x8000));
			m_asm.b(asmjit::arm::CondCode::kUnsignedGE, multiwrapModulo);
		}

		// modulo
		updateAddressRegisterSubModuloN1(_r, r32(m), r32(moduloMask), _addN);
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
		maskSC1624(_r);
	}

	void JitOps::updateAddressRegisterSubModuloN1(const JitReg32& _r, const JitReg32& _m, const JitReg32& _mMask, bool _addN) const
	{
		const RegScratch scratch(m_block);
		const auto p = r32(scratch);

		if (!_addN)
		{
			m_asm.ands(p, _r, _mMask);
			m_asm.sub(p, _r, 1);
			m_asm.add(_r, _r, _m);
			m_asm.csel(_r, p, _r, asmjit::arm::CondCode::kNE);
		}
		else
		{
			m_asm.sub(p, _r, _m);
			m_asm.tst(p, _mMask);
			m_asm.csinc(_r, p, _r, asmjit::arm::CondCode::kZero);
		}
	}

	void JitOps::updateAddressRegisterSubModulo(const JitReg32& r, const JitReg32& n, const JitReg32& m, const JitReg32& mMask, bool _addN) const
	{
		const ShiftReg shift(m_block);
		const RegScratch scratch(m_block);

		const auto lowerBound = r32(shift.get());
		const auto mod = r32(scratch);

		signextend24To32(n);

		m_asm.bic(lowerBound, r, mMask);

		if(_addN)
			m_asm.add(r, n);
		else
			m_asm.sub(r, n);

		m_asm.add(mod, m, asmjit::Imm(1));

		// if (n & mask) == 0
		//     mod = 0
		m_asm.ands(n, n, mMask);
		m_asm.csel(mod, n, mod, asmjit::arm::CondCode::kZero);

		// if (r < lowerbound)
		//     r += mod
		m_asm.add(n, r, mod);
		m_asm.cmp(r, lowerBound);
		m_asm.csel(r, n, r, asmjit::arm::CondCode::kLT);

		// if r > upperBound
		//     r -= mod
		m_asm.add(lowerBound, m);
		m_asm.sub(n, r, mod);
		m_asm.cmp(r, lowerBound);
		m_asm.csel(r, n, r, asmjit::arm::CondCode::kGT);
	}
}
#endif