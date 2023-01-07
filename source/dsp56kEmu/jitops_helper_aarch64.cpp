#include "jittypes.h"

#ifdef HAVE_ARM64

#include "jitops.h"
#include "dsp.h"
#include "agu.h"

namespace dsp56k
{
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
