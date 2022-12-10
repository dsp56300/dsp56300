#include "jittypes.h"

#ifdef HAVE_X86_64

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
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();
		
		const DspValue moduloMask = makeDspValueAguReg(m_block, JitDspRegPool::DspM0mask, _rrr);

		m_asm.cmp(r32(_m), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jnz(notLinear);

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

	void JitOps::getXY0(DspValue& _dst, const uint32_t _aluIndex, bool _signextend) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		if(_signextend)
		{
			if(m_asm.hasBMI2())
			{
				const DSPReg xyRef(m_block, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _aluIndex), true, false, true);
				m_asm.rorx(r64(_dst.get()), xyRef.r64(), asmjit::Imm(64-40));
				m_asm.sar(r64(_dst.get()), asmjit::Imm(40));
			}
			else
			{
				m_dspRegs.getXY(_dst.get(), _aluIndex);
				signextend24to64(r64(_dst.get()));
			}
		}
		else
		{
			m_dspRegs.getXY(_dst.get(), _aluIndex);
			m_asm.and_(_dst.get(), asmjit::Imm(0xffffff));
		}
	}

	void JitOps::getXY1(DspValue& _dst, const uint32_t _aluIndex, bool _signextend) const
	{
		if(!_dst.isRegValid())
		{
			_dst.temp(DspValue::Temp24);
		}
		else
		{
			assert(_dst.getBitCount() == 24);
		}

		if(_signextend)
		{
			if(m_asm.hasBMI2())
			{
				const DSPReg xyRef(m_block, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _aluIndex), true, false, true);
				m_asm.rorx(r64(_dst.get()), xyRef.r64(), asmjit::Imm(64-16));
			}
			else
			{
				m_dspRegs.getXY(r64(_dst.get()), _aluIndex);
				m_asm.shl(r64(_dst.get()), asmjit::Imm(16));
			}

			m_asm.sar(r64(_dst.get()), asmjit::Imm(40));
		}
		else
		{
			m_dspRegs.getXY(r64(_dst.get()), _aluIndex);
			m_asm.shr(r64(_dst.get()), asmjit::Imm(24));
		}
	}

	void JitOps::setXY0(const uint32_t _xy, const DspValue& _src)
	{
		const auto temp = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), true, true);
		m_asm.and_(temp, asmjit::Imm(0xffffffffff000000));

		if(_src.isImmediate())
		{
			if (_src.imm())
			{
				assert(_src.imm() <= 0xffffff);
				m_asm.or_(temp, asmjit::Imm(_src.imm()));
			}
		}
		else
		{
			m_asm.or_(temp, r64(_src.get()));
		}
	}

	void JitOps::setXY1(const uint32_t _xy, const DspValue& _src)
	{
		const RegGP shifted(m_block);

		if(_src.isImmediate())
		{
			m_asm.mov(shifted, asmjit::Imm(static_cast<uint64_t>(_src.imm24()) << 24ull));
		}
		else
		{
			_src.copyTo(shifted, 24);
			m_asm.shl(shifted, asmjit::Imm(24));
		}

		const auto temp = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), true, true);
		m_asm.and_(r32(temp), asmjit::Imm(0xffffff));
		m_asm.or_(temp, shifted.get());
	}

	void JitOps::getALU0(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		m_dspRegs.getALU(_dst.get(), _aluIndex);
		m_asm.and_(r32(_dst.get()), asmjit::Imm(0xffffff));
	}

	void JitOps::getALU1(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		m_dspRegs.getALU(r64(_dst.get()), _aluIndex);
		m_asm.shr(r64(_dst.get()), asmjit::Imm(24));
		m_asm.and_(r32(_dst.get()), asmjit::Imm(0xffffff));
	}

	void JitOps::setALU0(const uint32_t _aluIndex, const DspValue& _src) const
	{
		const RegGP maskedSource(m_block);
		_src.copyTo(maskedSource, 24);
		m_asm.and_(maskedSource, asmjit::Imm(0xffffff));

		AluRef temp(m_block, _aluIndex, true, true);

		m_asm.and_(temp.get(), asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp.get(), maskedSource.get());
	}

	void JitOps::setALU1(const uint32_t _aluIndex, const DspValue& _src) const
	{
		const RegGP maskedSource(m_block);
		_src.copyTo(maskedSource, 24);
		m_asm.and_(maskedSource, asmjit::Imm(0xffffff));

		AluRef temp(m_block, _aluIndex, true, true);;

		m_asm.ror(temp, asmjit::Imm(24));
		m_asm.and_(temp, asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp.get(), maskedSource.get());
		m_asm.rol(temp, asmjit::Imm(24));
	}

	void JitOps::setALU2(const uint32_t _aluIndex, const DspValue& _src) const
	{
		const RegGP maskedSource(m_block);
		_src.copyTo(maskedSource, 24);
		m_asm.and_(maskedSource, asmjit::Imm(0xff));

		AluRef temp(m_block, _aluIndex);

		m_asm.ror(temp, asmjit::Imm(48));
		m_asm.and_(temp.get(), asmjit::Imm(0xffffffffffffff00));
		m_asm.or_(temp.get(), maskedSource.get());
		m_asm.rol(temp, asmjit::Imm(48));
	}

	void JitOps::setSSH(const DspValue& _src) const
	{
		incSP();
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			m_asm.ror(_ss, asmjit::Imm(24));
			m_asm.and_(_ss, asmjit::Imm(0xffffffffff000000));
			if(_src.isImmediate())
			{
				if (_src.imm24())
					m_asm.or_(_ss, asmjit::Imm(_src.imm24()));
			}
			else
			{
				m_asm.or_(_ss, r64(_src));
			}
			m_asm.rol(_ss, asmjit::Imm(24));
		}, true, true);
	}

	void JitOps::setSSL(const DspValue& _src) const
	{
		m_dspRegs.modifySS([&](const JitReg64& _ss)
		{
			m_asm.and_(_ss, asmjit::Imm(0xffffffffff000000));

			if(_src.isImmediate())
			{
				if(_src.imm24())
					m_asm.or_(_ss, asmjit::Imm(_src.imm24()));
			}
			else
			{
				m_asm.or_(_ss, r64(_src));
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
			else
			{
				if(_ssh.isImmediate())
				{
					m_asm.mov(r64(_ss), asmjit::Imm(static_cast<uint64_t>(_ssh.imm24()) << 24ull));
				}
				else
				{
					m_asm.mov(r32(_ss), r32(_ssh));
					m_asm.shl(r64(_ss), asmjit::Imm(24));
				}
				m_asm.or_(r64(_ss), r64(_ssl));
			}
		}, false, true);
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
		m_asm.dec(m_block.dspRegPool().makeDspPtr(m_block.dsp().regs().sp));
		m_asm.dec(m_block.dspRegPool().makeDspPtr(m_block.dsp().regs().sc));
	}

	void JitOps::incSP() const
	{
		m_asm.inc(m_block.dspRegPool().makeDspPtr(m_block.dsp().regs().sp));
		m_asm.inc(m_block.dspRegPool().makeDspPtr(m_block.dsp().regs().sc));
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
/*
		if(mode)
		{
			// test code to detect DSP mode issues, compares the DSP mode SR to the actual SR and triggers a break interrupt if it doesn't match
			JitDspMode testMode;
			testMode.initialize(m_block.dsp());

			const auto a = mode->getSR() & ~SR_LF;
			const auto b = testMode.getSR() & ~SR_LF;

			assert(a == b);

			const auto l = m_asm.newLabel();

			const auto temp = regReturnVal;
			const auto t = r32(temp);

			m_asm.mov(t, r32(getSR(JitDspRegs::Read)));
			m_asm.and_(t, (0xffff00 & ~SR_LF));
			m_asm.cmp(t, mode->getSR() & ~SR_LF);
			m_asm.jz(l);
			m_asm.int3();
			m_asm.bind(l);
		}
*/
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
			const ShiftReg s0s1(m_block);

			sr_getBitValue(s0s1, SRB_S1);
			m_asm.shl(_dst, s0s1.get().r8());

			sr_getBitValue(s0s1, SRB_S0);
			m_asm.shr(_dst, s0s1.get().r8());
		}

		{
			// non-limited default
			m_asm.shr(_dst, asmjit::Imm(24));

			const RegGP tester(m_block);
			m_asm.mov(r32(tester), r32(_dst));

			// lower limit
			m_asm.cmp(r32(tester), asmjit::Imm(0xff800000));
			{
				const RegScratch minmax(m_block);
				m_asm.mov(r32(minmax), 0x800000);
				m_asm.cmovl(r32(_dst), r32(minmax));
			}

			// upper limit
			m_asm.cmp(r32(tester), asmjit::Imm(0x007fffff));
			{
				const RegScratch minmax(m_block);
				m_asm.mov(r32(minmax), 0x007fffff);
				m_asm.cmovg(r32(_dst), r32(minmax));
			}

			m_asm.cmp(r32(tester), r32(_dst));
			ccr_update_ifNotZero(CCRB_L);
			m_asm.and_(r32(_dst), asmjit::Imm(0x00ffffff));
		}
	}
}

#endif
