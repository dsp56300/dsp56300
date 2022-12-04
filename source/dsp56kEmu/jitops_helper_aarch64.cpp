#include "jittypes.h"

#ifdef HAVE_ARM64

#include "jitops.h"
#include "dsp.h"
#include "agu.h"

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

		const auto linear = m_asm.newLabel();
		const auto notLinear = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		const bool mIsConstant=false;
		if (mIsConstant)
		{
			const TWord mval=0;	// fill in value of _m here.
			if (mval==0xFFFFFF)
			{
				if (_addN)	m_asm.add(_r, _n);
				else		m_asm.sub(_r, _n);
			}
			else if (!(mval&0xFFFF))	updateAddressRegisterSubBitreverse(_r, _n, _addN);
			else if (mval&0x8000)	{}	// multi-wrap modulo mode
			else
			{
				const TWord modmask=AGU::calcModuloMask(mval);
				const TWord mplusone=mval+1;
				const ShiftReg shifter(m_block);

				const RegScratch scratch(m_block);
				const auto p = r32(scratch), temp = r32(shifter.get()), r=r32(_r), n=r32(_n), m=r32(_m);

				m_asm.and_(p, r, asmjit::Imm(modmask));
				if (_addN)	m_asm.add(r, r, n);
				else		m_asm.sub(r, r, n);

				if (m_block.getConfig().aguAssumePositiveN)
				{
					m_asm.cmp(n, mval);
				}
				else
				{
					m_asm.test_(n);
					m_asm.cneg(temp, r32(_n), asmjit::arm::CondCode::kLT);
					m_asm.cmp(temp, _m);									// modulo or linear
				}
				m_asm.b(asmjit::arm::CondCode::kGT, end);

				if (_addN)	m_asm.adds(p, p, n);
				else		m_asm.subs(p, r, n);

				m_asm.add(temp, r, asmjit::Imm(mplusone));
				m_asm.csel(r, temp, r, asmjit::arm::CondCode::kLT);
				m_asm.sub(temp, r, asmjit::Imm(mplusone));
				m_asm.cmp(p, asmjit::Imm(mval));
				m_asm.csel(r, temp, r, asmjit::arm::CondCode::kGT);
				m_asm.bind(end);
			}
			m_asm.and_(_r, asmjit::Imm(0xffffff));
			return;
		}

		const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);

		{
			const RegScratch scratch(m_block);
			m_asm.mov(r32(scratch), asmjit::Imm(0xffffff));	// linear shortcut
			m_asm.cmp(r32(_m), r32(scratch));
			m_asm.jnz(notLinear);
		}

		// linear:
		m_asm.bind(linear);
		if (_addN)
			m_asm.add(_r, _n);
		else
			m_asm.sub(_r, _n);
		m_asm.jmp(end);

		m_asm.bind(notLinear);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			m_asm.tst(_m, asmjit::Imm(0xffff));						// bit reverse
			m_asm.jz(bitreverse);
		}

		{
			const RegScratch scratch(m_block);
			m_asm.and_(r32(scratch), _m, asmjit::Imm(0xffff));
			m_asm.cmp(r32(scratch), asmjit::Imm(0x8000));
		}
		m_asm.b(asmjit::arm::CondCode::kUnsignedGE, multipleWrapModulo);

		{
			const RegScratch scratch(m_block);
			const auto nAbs = r32(scratch);							// compare abs(n) with m
			m_asm.test_(r32(_n));
			m_asm.cneg(nAbs, r32(_n), asmjit::arm::CondCode::kLT);
			m_asm.cmp(nAbs, r32(moduloMask));						// modulo or linear
			m_asm.b(asmjit::arm::CondCode::kGT, linear);
		}

		// modulo:
		m_asm.bind(modulo);
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

		const bool mIsConstant=false;
		if (mIsConstant)
		{
			const TWord mval=0;	// fill in value of _m here.
			if (mval==0xFFFFFF)
			{
				if (_addN)	m_asm.inc(_r);
				else		m_asm.dec(_r);
			}
			else if (!(mval&0xFFFF))	updateAddressRegisterSubBitreverseN1(_r, _addN);
			else if (mval&0x8000)		{}	// multi-wrap modulo mode
			else						updateAddressRegisterSubModuloN1(_r, _m, r32(moduloMask), _addN);
			m_asm.and_(_r, asmjit::Imm(0xffffff));
			return;
		}
		
		const auto notLinear = m_asm.newLabel();
		const auto end = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto multiwrapModulo = m_asm.newLabel();

		{
			const RegScratch scratch(m_block);
			m_asm.mov(r32(scratch), asmjit::Imm(0xffffff));		// linear shortcut
			m_asm.cmp(r32(_m), r32(scratch));
			m_asm.jnz(notLinear);
		}

		if (_addN)
			m_asm.inc(_r);
		else
			m_asm.dec(_r);

		m_asm.jmp(end);

		m_asm.bind(notLinear);

		if (m_block.getConfig().aguSupportBitreverse)
		{
			m_asm.tst(_m, asmjit::Imm(0xffff));							// bit reverse
			m_asm.jz(bitreverse);
		}

		{
			const RegScratch scratch(m_block);
			m_asm.and_(r32(scratch), _m, asmjit::Imm(0xffff));		// multiple-wrap modulo
			m_asm.cmp(r32(scratch), asmjit::Imm(0x8000));
			m_asm.b(asmjit::arm::CondCode::kUnsignedGE, multiwrapModulo);
		}

		// modulo
		updateAddressRegisterSubModuloN1(_r, _m, r32(moduloMask), _addN);
		m_asm.jmp(end);

		// bitreverse
		if (m_block.getConfig().aguSupportBitreverse)
		{
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
			m_asm.ands(p, _r, _mMask);
			m_asm.sub(p, _r, 1);
			m_asm.add(_r, _r, _m);
			m_asm.csel(_r, p, _r, asmjit::arm::CondCode::kNE);
		}
		else
		{
			m_asm.and_(p, _r, _mMask);
			m_asm.cmp(p, _m);
			m_asm.mov(p, asmjit::Imm(-1));
			m_asm.csel(p, _m, p, asmjit::arm::CondCode::kEQ);
			m_asm.sub(_r, p);
		}
	}

	void JitOps::updateAddressRegisterSubModulo(const JitReg32& r, const JitReg32& n, const JitReg32& m, const JitReg32& mMask, bool _addN) const
	{
		const ShiftReg shifter(m_block);
		const auto& p64 = shifter;
		const auto p = r32(p64.get());

		const RegScratch scratch(m_block);
		const auto temp = r32(scratch);

		m_asm.and_(p, r, mMask);
		m_asm.add(m, m, asmjit::Imm(1));
		if (_addN)
		{
			m_asm.add(r, r, n);
			m_asm.adds(p, p, n);
		}
		else
		{
			m_asm.sub(r, r, n);
			m_asm.subs(p, p, n);
		}
		m_asm.add(temp, r, m);
		m_asm.csel(r, temp, r, asmjit::arm::CondCode::kMI);
		m_asm.sub(temp, r, m);
		m_asm.sub(m, m, asmjit::Imm(1));
		m_asm.cmp(p,m);
		m_asm.csel(r, temp, r, asmjit::arm::CondCode::kGT);
	}



	void JitOps::getXY0(DspValue& _dst, const uint32_t _aluIndex, bool _signextend) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		const auto& src = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _aluIndex), true, false);
		if(_signextend)
			m_asm.sbfx(r64(_dst), r64(src), asmjit::Imm(0), asmjit::Imm(24));
		else
			m_asm.ubfx(r64(_dst), r64(src), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitOps::getXY1(DspValue& _dst, const uint32_t _aluIndex, bool _signextend) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		const auto& src = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _aluIndex), true, false);
		if(_signextend)
			m_asm.sbfx(r64(_dst), r64(src), asmjit::Imm(24), asmjit::Imm(24));
		else
			m_asm.ubfx(r64(_dst), r64(src), asmjit::Imm(24), asmjit::Imm(24));
	}

	void JitOps::setXY0(const uint32_t _xy, const DspValue& _src)
	{
		const auto temp = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), true, true);

		if(_src.isImmediate())
		{
			if(_src.imm())
			{
				const RegScratch tempSrc(m_block);
				m_asm.mov(tempSrc, asmjit::Imm(_src.imm()));
				m_asm.bfi(r64(temp), r64(tempSrc), asmjit::Imm(0), asmjit::Imm(24));
			}
			else
			{
				m_asm.bfi(r64(temp), asmjit::a64::regs::xzr, asmjit::Imm(0), asmjit::Imm(24));
			}
		}
		else
		{
			m_asm.bfi(r64(temp), r64(_src), asmjit::Imm(0), asmjit::Imm(24));
		}
	}

	void JitOps::setXY1(const uint32_t _xy, const DspValue& _src)
	{
		const auto temp = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), true, true);
		if (_src.isImmediate())
		{
			if (_src.imm())
			{
				const RegScratch tempSrc(m_block);
				m_asm.mov(tempSrc, asmjit::Imm(_src.imm()));
				m_asm.bfi(r64(temp), r64(tempSrc), asmjit::Imm(24), asmjit::Imm(24));
			}
			else
			{
				m_asm.bfi(r64(temp), asmjit::a64::regs::xzr, asmjit::Imm(24), asmjit::Imm(24));
			}
		}
		else
		{
			m_asm.bfi(r64(temp), r64(_src), asmjit::Imm(24), asmjit::Imm(24));
		}
	}

	void JitOps::getALU0(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		AluRef src(m_block, _aluIndex, true, false);
		m_asm.ubfx(r64(_dst), r64(src), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitOps::getALU1(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		AluRef src(m_block, _aluIndex, true, false);
		m_asm.ubfx(r64(_dst), r64(src), asmjit::Imm(24), asmjit::Imm(24));
	}

	void JitOps::setALU0(const uint32_t _aluIndex, const DspValue& _src) const
	{
		AluRef d(m_block, _aluIndex, true, true);

		if(_src.isImmediate())
		{
			const RegGP t(m_block);
			m_asm.mov(t, asmjit::Imm(_src.imm24()));
			m_asm.bfi(d, r64(t), asmjit::Imm(0), asmjit::Imm(24));
		}
		else
		{
			m_asm.bfi(d, r64(_src), asmjit::Imm(0), asmjit::Imm(24));
		}
	}

	void JitOps::setALU1(const uint32_t _aluIndex, const DspValue& _src) const
	{
		AluRef d(m_block, _aluIndex, true, true);
		if (_src.isImmediate())
		{
			const RegGP t(m_block);
			m_asm.mov(t, asmjit::Imm(_src.imm24()));
			m_asm.bfi(d, r64(t), asmjit::Imm(24), asmjit::Imm(24));
		}
		else
		{
			m_asm.bfi(d, r64(_src), asmjit::Imm(24), asmjit::Imm(24));
		}
	}

	void JitOps::setALU2(const uint32_t _aluIndex, const DspValue& _src) const
	{
		AluRef d(m_block, _aluIndex, true, true);
		if (_src.isImmediate())
		{
			const RegGP t(m_block);
			m_asm.mov(t, asmjit::Imm(_src.imm24()));
			m_asm.bfi(d, r64(t), asmjit::Imm(48), asmjit::Imm(8));
		}
		else
		{
			m_asm.bfi(d, r64(_src), asmjit::Imm(48), asmjit::Imm(8));
		}
	}

	void JitOps::setSSH(const DspValue& _src) const
	{
		incSP();
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			if (_src.isImmediate())
			{
				const RegGP t(m_block);
				m_asm.mov(r32(t), asmjit::Imm(_src.imm24()));
				m_asm.bfi(_ss, r64(t), asmjit::Imm(24), asmjit::Imm(24));
			}
			else
			{
				m_asm.bfi(_ss, r64(_src), asmjit::Imm(24), asmjit::Imm(24));
			}
		}, true, true);
	}

	void JitOps::setSSL(const DspValue& _src) const
	{
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			if (_src.isImmediate())
			{
				const RegGP t(m_block);
				m_asm.mov(r32(t), asmjit::Imm(_src.imm24()));
				m_asm.bfi(_ss, r64(t), asmjit::Imm(0), asmjit::Imm(24));
			}
			else
			{
				m_asm.bfi(_ss, r64(_src), asmjit::Imm(0), asmjit::Imm(24));
			}
		}, true, true);
	}

	void JitOps::setSSHSSL(const DspValue& _ssh, const DspValue& _ssl) const
	{
		incSP();
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			if(_ssh.isImmediate() && _ssl.isImmediate())
			{
				m_asm.mov(r64(_ss), asmjit::Imm((static_cast<uint64_t>(_ssh.imm24()) << 24ull) | static_cast<uint64_t>(_ssl.imm24())));
			}
			else if(_ssh.isImmediate())
			{
				m_asm.mov(r32(_ss), asmjit::Imm(_ssh.imm24()));
				m_asm.orr(r64(_ss), r64(_ssl), r64(_ss), asmjit::arm::lsl(24));
			}
			else
			{
				m_asm.orr(r64(_ss), r64(_ssl), r64(_ssh), asmjit::arm::lsl(24));
			}
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

		const auto* mode = m_block.getMode();

		if(mode)
		{
			int shift = 0;
			if(mode->testSR(SRB_S1))
				++shift;
			if(mode->testSR(SRB_S0))
				--shift;
			if(shift > 0)
				m_asm.add(_dst, _dst);
			else if(shift < 0)
				m_asm.shr(_dst, asmjit::Imm(1));
		}
		else
		{
			const ShiftReg shifter(m_block);
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S1);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.lsl(_dst, _dst, shifter.get());

			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S0);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.lsr(_dst, _dst, shifter.get());
		}
		
		{
			// non-limited default
			m_asm.shr(r64(_dst), asmjit::Imm(24));

			const RegGP tester(m_block);
			m_asm.mov(r32(tester), r32(_dst));

			// lower limit
			{
				const RegScratch limit(m_block);
				m_asm.mov(r32(limit), asmjit::Imm(0xff800000));
				m_asm.cmp(r32(tester), r32(limit));
				m_asm.csel(r32(_dst), r32(limit), r32(_dst), asmjit::arm::CondCode::kLT);
			}

			// upper limit
			{
				const RegScratch limit(m_block);
				m_asm.mov(r32(limit), asmjit::Imm(0x007fffff));
				m_asm.cmp(r32(tester), r32(limit));
				m_asm.csel(r32(_dst), r32(limit), r32(_dst), asmjit::arm::CondCode::kGT);
			}

			m_asm.cmp(r32(tester), r32(_dst));
			ccr_update_ifNotZero(CCRB_L);
			m_asm.and_(r32(_dst), asmjit::Imm(0x00ffffff));
		}
	}
}

#endif
