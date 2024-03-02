#include "jittypes.h"

#ifdef HAVE_ARM64

#include "jitops.h"
#include "dsp.h"
#include "agu.h"

namespace dsp56k
{
	void JitOps::getALU0(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		m_asm.ubfx(r64(_dst), r64(m_dspRegs.getALU(_aluIndex)), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitOps::getALU1(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		m_asm.ubfx(r64(_dst), r64(m_dspRegs.getALU(_aluIndex)), asmjit::Imm(24), asmjit::Imm(24));
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
		const auto& r = m_block.dspRegPool().get(PoolReg::DspSR, true, true);
		m_asm.bfi(r, _src, asmjit::Imm(8), asmjit::Imm(8));
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

	void JitOps::transferSaturation24(const JitReg64& _dst, const JitReg64& _src)
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
			int shift = 24;
			if(mode->testSR(SRB_S1))
				--shift;
			if(mode->testSR(SRB_S0))
				++shift;
			m_asm.asr(_dst, _src, asmjit::Imm(shift));
		}
		else
		{
			const ShiftReg shifter(m_block);
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S1);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.lsl(_dst, _src, shifter.get());

			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S0);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.asr(_dst, _dst, shifter.get());

			m_asm.asr(r64(_dst), r64(_dst), asmjit::Imm(24));
		}
		
		{
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
				m_asm.mvn(r32(limit), r32(limit)); // = 0x007fffff
				m_asm.cmp(r32(tester), r32(limit));
				m_asm.csel(r32(_dst), r32(limit), r32(_dst), asmjit::arm::CondCode::kGT);
			}

			m_asm.cmp(r32(tester), r32(_dst));
			ccr_update_ifNotZero(CCRB_L);
			m_asm.and_(r32(_dst), asmjit::Imm(0x00ffffff));
		}
	}

	void JitOps::transferSaturation48(const JitReg64& _dst, const JitReg64& _src)
	{
		// scaling

		/*
		if( sr_test_noCache(SR_S1) )
			_scale.var <<= 1;
		else if( sr_test_noCache(SR_S0) )
			_scale.var >>= 1;
		*/

		const auto* mode = m_block.getMode();

		m_asm.lsl(_dst, _src, asmjit::Imm(8));

		if(mode)
		{
			int shift = 8;
			if(mode->testSR(SRB_S1))
				--shift;
			if(mode->testSR(SRB_S0))
				++shift;
			m_asm.asr(_dst, _dst, asmjit::Imm(shift));
		}
		else
		{
			const ShiftReg shifter(m_block);
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S1);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.lsl(_dst, _dst, shifter.get());

			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read), SRB_S0);
			m_asm.cset(shifter, asmjit::arm::CondCode::kNotZero);
			m_asm.asr(_dst, _dst, shifter.get());

			m_asm.asr(_dst, _dst, asmjit::Imm(8));
		}
		
		{
			const RegGP tester(m_block);
			m_asm.mov(r64(tester), r64(_dst));

			{
				const RegScratch limit(m_block);

				// lower limit
				m_asm.mov(r64(limit), asmjit::Imm(0xffff800000000000));
				m_asm.cmp(r64(tester), r64(limit));
				m_asm.csel(r64(_dst), r64(limit), r64(_dst), asmjit::arm::CondCode::kLT);

				// upper limit
				m_asm.mvn(r64(limit), r64(limit)); // = 0x00007fffffffffff
				m_asm.cmp(r64(tester), r64(limit));
				m_asm.csel(r64(_dst), r64(limit), r64(_dst), asmjit::arm::CondCode::kGT);
			}

			m_asm.cmp(r64(tester), r64(_dst));
			ccr_update_ifNotZero(CCRB_L);
			m_dspRegs.mask48(_dst);
		}
	}
}

#endif
