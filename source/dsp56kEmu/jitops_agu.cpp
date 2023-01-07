#include "jitops.h"

#include "jitconfig.h"

namespace dsp56k
{

	void JitOps::updateAddressRegisterSub(const AddressingMode _mode, const JitReg32& _r, const JitReg32& _n, const JitReg32& _m, uint32_t _rrr, bool _addN)
	{
//		assert(JitDspMode::calcAddressingMode(m_block.dsp().regs().m[_rrr]) == _mode);

		const auto execLinear = [&]()
		{
			if (_addN)
				m_asm.add(_r, _n);
			else
				m_asm.sub(_r, _n);
		};

		switch (_mode)
		{
		case AddressingMode::Linear:
			execLinear();
			break;
		case AddressingMode::Modulo:
			{
				const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);
				updateAddressRegisterSubModulo(r32(_r), _n, r32(_m), r32(moduloMask), _addN);
			}
			break;
		case AddressingMode::MultiWrapModulo:
			{
				const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);
				updateAddressRegisterSubMultipleWrapModulo(_r, _n, r32(moduloMask), _addN);
			}
			break;
		case AddressingMode::Bitreverse: 
			updateAddressRegisterSubBitreverse(_r, _n, _addN);
			break;
		default:
			break;
		}

		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	void JitOps::updateAddressRegisterSubN1(const AddressingMode _mode, const JitReg32& _r, const JitReg32& _m, uint32_t _rrr, bool _addN)
	{
//		assert(JitDspMode::calcAddressingMode(m_block.dsp().regs().m[_rrr]) == _mode);

		switch (_mode)
		{
		case AddressingMode::Linear:
			if (_addN)	m_asm.inc(_r);
			else		m_asm.dec(_r);
			break;
		case AddressingMode::Modulo:
			{
				const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);
				updateAddressRegisterSubModuloN1(_r, _m, r32(moduloMask), _addN);
			}
			break;
		case AddressingMode::MultiWrapModulo:
			{
				const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);
				updateAddressRegisterSubMultipleWrapModuloN1(_r, _addN, r32(moduloMask));
			}
			break;
		case AddressingMode::Bitreverse: 
			updateAddressRegisterSubBitreverseN1(_r, _addN);
			break;
		default: 
			break;
		}

		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	DspValue JitOps::updateAddressRegister(const TWord _mmm, const TWord _rrr, bool _writeR/* = true*/, bool _returnPostR/* = false*/)
	{
		if (_mmm == 6)													/* 110         */
		{
			return DspValue(m_block, getOpWordB(), DspValue::Immediate24);
		}

		if (_mmm == 4)													/* 100 (Rn)    */
		{
			return DspValue(m_block, JitDspRegPool::DspR0, true, false, _rrr);
		}

		DspValue m(m_block);
		m_dspRegs.getM(m, _rrr);

		if (_mmm == 7)													/* 111 -(Rn)   */
		{
			if (_writeR)
			{
				{
					DspValue r = makeDspValueRegR(m_block, _rrr, true, true);
					updateAddressRegisterSubN1(r32(r.get()), r32(m.get()), _rrr, false);
				}
				return DspValue(m_block, JitDspRegPool::DspR0, true, false, _rrr);
			}

			DspValue r = m_dspRegs.getR(_rrr);
			r.toTemp();
			updateAddressRegisterSubN1(r32(r.get()), r32(m.get()), _rrr, false);
			return DspValue(std::move(r));
		}

		DspValue rRef = makeDspValueRegR(m_block, _rrr);

		if (_mmm == 5)													/* 101 (Rn+Nn) */
		{
			DspValue n(m_block);
			m_dspRegs.getN(n, _rrr);
			n.toTemp();

			DspValue r(m_block);
			r.temp(DspValue::Temp24);

			m_asm.mov(r32(r.get()), r32(rRef.get()));
			updateAddressRegisterSub(r32(r.get()), r32(n.get()), r32(m.get()), _rrr, true);
			return DspValue(std::move(r));
		}

		DspValue dst(m_block);

		if (!_returnPostR)
		{
			if (!_writeR)
				return DspValue(m_block, JitDspRegPool::DspR0, true, false, _rrr);

			dst.temp(DspValue::Temp24);
			m_asm.mov(dst.get(), r32(rRef.get()));
		}

		JitReg32 r;

		if (!_writeR)
		{
			dst.temp(DspValue::Temp24);
			r = r32(dst.get());
			m_asm.mov(r, r32(rRef.get()));
		}
		else
			r = r32(rRef);

		if (_mmm == 0)													/* 000 (Rn)-Nn */
		{
			DspValue n(m_block);
			m_dspRegs.getN(n, _rrr);
			n.toTemp();
			updateAddressRegisterSub(r, r32(n.get()), r32(m.get()), _rrr, false);
		}
		if (_mmm == 1)													/* 001 (Rn)+Nn */
		{
			DspValue n(m_block);
			m_dspRegs.getN(n, _rrr);
			n.toTemp();
			updateAddressRegisterSub(r, r32(n.get()), r32(m.get()), _rrr, true);
		}
		if (_mmm == 2)													/* 010 (Rn)-   */
		{
			updateAddressRegisterSubN1(r, r32(m.get()), _rrr, false);
		}
		if (_mmm == 3)													/* 011 (Rn)+   */
		{
			updateAddressRegisterSubN1(r, r32(m.get()), _rrr, true);
		}

		if (_writeR)
		{
			rRef.write();
			rRef.release();

			if (_returnPostR)
				return DspValue(m_block, JitDspRegPool::DspR0, true, true, _rrr);
		}

		return DspValue(std::move(dst));
	}

	void JitOps::updateAddressRegisterSubMultipleWrapModulo(const JitReg32& _r, const JitReg32& _n, const JitReg32& _mask, const bool _addN)
	{
		const RegScratch scratch(m_block);
		const auto temp = r32(scratch);

#ifdef HAVE_ARM64
		m_asm.and_(temp, _r, _mask);// int32_t temp = r & moduloMask;
#else
		m_asm.mov(temp, _r);		// int32_t temp = r & moduloMask;
		m_asm.and_(temp, _mask);
#endif
		m_asm.xor_(_r, temp);		// r ^= temp;
		if(_addN)
			m_asm.add(temp, _n);	// temp += n;
		else
			m_asm.sub(temp, _n);	// temp -= n;
		m_asm.and_(temp, _mask);	// temp &= moduloMask;
		m_asm.or_(_r, temp);		// r |= temp;
	}

	void JitOps::updateAddressRegisterSubMultipleWrapModuloN1(const JitReg32& _r, const bool _addN, const JitReg32& _mask)
	{
		const RegScratch scratch(m_block);
		const auto temp = r32(scratch);

		m_asm.mov(temp, _r);		// int32_t temp = r & moduloMask;
		m_asm.and_(temp, _mask);
		m_asm.xor_(_r, temp);		// r ^= temp;
		if(_addN)
			m_asm.inc(temp);		// temp += n;
		else
			m_asm.dec(temp);		// temp -= n;
		m_asm.and_(temp, _mask);	// temp &= moduloMask;
		m_asm.or_(_r, temp);		// r |= temp;
	}

	void JitOps::updateAddressRegisterSubBitreverse(const JitReg32& _r, const JitReg32& _n, bool _addN)
	{
		if (!m_block.getConfig().aguSupportBitreverse)
			return;

		bitreverse24(_r);
		bitreverse24(_n);
		if (_addN)
			m_asm.add(_r, _n);
		else
			m_asm.sub(_r, _n);
		bitreverse24(_r);
	}

	void JitOps::updateAddressRegisterSubBitreverseN1(const JitReg32& _r, const bool _addN)
	{
		if (!m_block.getConfig().aguSupportBitreverse)
			return;

		bitreverse24(_r);
		const auto n = dsp56k::bitreverse24(static_cast<TWord>(1));
#ifdef HAVE_ARM64
		{
			const RegScratch scratch(m_block);
			m_asm.mov(r32(scratch), asmjit::Imm(n));
			if (_addN)
				m_asm.add(_r, r32(scratch));
			else
				m_asm.sub(_r, r32(scratch));
		}
#else
		if (_addN)
			m_asm.add(_r, asmjit::Imm(n));
		else
			m_asm.sub(_r, asmjit::Imm(n));
#endif
		bitreverse24(_r);
	}
}