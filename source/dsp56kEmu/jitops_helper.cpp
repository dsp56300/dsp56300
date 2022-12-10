#include "jitconfig.h"
#include "jitops.h"
#include "dsp.h"

namespace dsp56k
{
	void dspExecDefaultPreventInterrupt(DSP*);
	void dspExecNop(DSP*);

	void JitOps::signextend56to64(const JitReg64& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(56));
#else
		m_asm.sal(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
#endif
	}

	void JitOps::signextend48to64(const JitReg64& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(48));
#else
		m_asm.sal(_reg, asmjit::Imm(16));
		m_asm.sar(_reg, asmjit::Imm(16));
#endif
	}

	void JitOps::signextend48to56(const JitReg64& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(48));
		m_asm.ubfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(56));
#else
		m_asm.sal(_reg, asmjit::Imm(16));
		m_asm.sar(_reg, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
#endif
	}

	void JitOps::signextend24to56(const JitReg64& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(24));
		m_asm.ubfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(56));
#else
		m_asm.sal(_reg, asmjit::Imm(40));
		m_asm.sar(_reg, asmjit::Imm(32));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
#endif
	}

	void JitOps::signextend24to64(const JitReg64& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(24));
#else
		m_asm.sal(_reg, asmjit::Imm(40));
		m_asm.sar(_reg, asmjit::Imm(40));
#endif
	}

	void JitOps::signextend24To32(const JitReg32& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(24));
#else
		m_asm.shl(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
#endif
	}

	void JitOps::bitreverse24(const JitReg32& x) const
	{
		/*
		x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
		x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
		x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));

		return ((x & 0xff0000) >> 16) | (x & 0x00ff00) | ((x & 0x0000ff) << 16);
		*/
#ifdef HAVE_ARM64
		m_asm.rbit(x, x);
		m_asm.lsr(x, x, asmjit::Imm(8));
#else
		const RegScratch temp(m_block);
		const auto t = r32(temp);

		m_asm.mov(t, x);
		m_asm.and_(t, asmjit::Imm(0xaaaaaaaa));
		m_asm.and_(x, asmjit::Imm(0x55555555));
		m_asm.shr(t, asmjit::Imm(1));
		m_asm.shl(x, asmjit::Imm(1));
		m_asm.or_(x, t);

		m_asm.mov(t, x);
		m_asm.and_(t, asmjit::Imm(0xcccccccc));
		m_asm.and_(x, asmjit::Imm(0x33333333));
		m_asm.shr(t, asmjit::Imm(2));
		m_asm.shl(x, asmjit::Imm(2));
		m_asm.or_(x, t);

		m_asm.mov(t, x);
		m_asm.and_(t, asmjit::Imm(0xf0f0f0f0));
		m_asm.and_(x, asmjit::Imm(0x0f0f0f0f));
		m_asm.shr(t, asmjit::Imm(4));
		m_asm.shl(x, asmjit::Imm(4));
		m_asm.or_(x, t);

		m_asm.bswap(x);
		m_asm.shr(x, asmjit::Imm(8));
#endif
	}

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

	void JitOps::signed24To56(const JitReg64& _r) const
	{
		m_asm.shl(_r, asmjit::Imm(40));
		m_asm.sar(_r, asmjit::Imm(8));		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_r, asmjit::Imm(8));
	}

	void JitOps::pushPCSR()
	{
		DspValue pc(m_block);

		if (m_fastInterrupt)
		{
			pc = m_block.dspRegPool().read(JitDspRegPool::DspPC);
		}
		else
		{
			pc.set(m_pcCurrentOp + m_opSize, DspValue::Immediate24);
		}

		DspValue sr(m_block);
		getSR(sr);
		setSSHSSL(pc, sr);
	}
	void JitOps::popPCSR()
	{
		{
			DspValue sr(m_block);
			getSSL(sr);
			setSR(sr);
		}
		popPC();
	}
	void JitOps::popPC()
	{
		DspValue pc(m_block);
		getSSH(pc);
		m_dspRegs.setPC(pc);
	}

	void JitOps::setDspProcessingMode(uint32_t _mode)
	{
		const DspValue r(m_block, _mode, DspValue::Immediate24);

		if constexpr (sizeof(m_block.dsp().m_processingMode) == sizeof(uint32_t))
			m_block.mem().mov(reinterpret_cast<uint32_t&>(m_block.dsp().m_processingMode), r);
		else if constexpr (sizeof(m_block.dsp().m_processingMode) == sizeof(uint64_t))
			m_block.mem().mov(reinterpret_cast<uint64_t&>(m_block.dsp().m_processingMode), r);

		static_assert(sizeof(m_block.dsp().m_interruptFunc) == 8);

		if(_mode == DSP::DefaultPreventInterrupt)
		{
			const auto* ptr = asmjit::func_as_ptr(&dspExecDefaultPreventInterrupt);
			m_block.mem().mov(&m_block.dsp().m_interruptFunc, reinterpret_cast<uint64_t>(ptr));
		}
		else if(_mode == DSP::LongInterrupt)
		{
			const auto* ptr = asmjit::func_as_ptr(&dspExecNop);
			m_block.mem().mov(&m_block.dsp().m_interruptFunc, reinterpret_cast<uint64_t>(ptr));
		}
		else
			assert(false && "support missing");
	}

	TWord JitOps::getOpWordB()
	{
		++m_opSize;
		assert(m_opSize == 2);
		return m_opWordB;
	}

	void JitOps::getOpWordB(DspValue& _dst)
	{
		if(_dst.isRegValid())
		{
			assert(_dst.getBitCount() == 24);
			m_asm.mov(_dst.get(), asmjit::Imm(getOpWordB()));
		}
		else
		{
			_dst.set(getOpWordB(), DspValue::Immediate24);
		}
	}

	void JitOps::getMR(const JitReg64& _dst) const
	{
#ifdef HAVE_ARM64
		m_asm.ubfx(r32(_dst), r32(m_dspRegs.getSR(JitDspRegs::Read)), asmjit::Imm(8), asmjit::Imm(8));
#else
		m_asm.movzx(_dst, m_dspRegs.getSR(JitDspRegs::Read).r16());
		m_asm.shr(_dst, asmjit::Imm(8));
#endif
	}

	void JitOps::getCCR(RegGP& _dst)
	{
		_dst.release();
		updateDirtyCCR();
		_dst.acquire();
#ifdef HAVE_ARM64
		m_asm.ubfx(r32(_dst), r32(m_dspRegs.getSR(JitDspRegs::Read)), asmjit::Imm(0), asmjit::Imm(8));
#else
		m_asm.movzx(r64(_dst), m_dspRegs.getSR(JitDspRegs::Read).r8());
#endif
	}

	void JitOps::getCOM(const JitReg64& _dst) const
	{
		m_block.dspRegPool().movDspReg(_dst, m_block.dsp().regs().omr);
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::getEOM(const JitReg64& _dst) const
	{
		m_block.dspRegPool().movDspReg(_dst, m_block.dsp().regs().omr);
		m_asm.shr(_dst, asmjit::Imm(8));
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::setCCR(const JitReg64& _src) const
	{
		m_ccrDirty = static_cast<CCRMask>(0);
#ifdef HAVE_ARM64
		m_asm.bfi(r32(m_dspRegs.getSR(JitDspRegs::ReadWrite)), r32(_src), asmjit::Imm(0), asmjit::Imm(8));
#else
		m_asm.mov(m_dspRegs.getSR(JitDspRegs::ReadWrite).r8(), _src.r8());
#endif
		}

	void JitOps::setCOM(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xffff00));
		m_asm.or_(r, _src);
		m_block.dspRegPool().movDspReg(m_block.dsp().regs().omr, r);
	}

	void JitOps::getSR(DspValue& _dst)
	{
		updateDirtyCCR();
		m_dspRegs.getSR(_dst);
	}

	JitRegGP JitOps::getSR(JitDspRegs::AccessType _accessType)
	{
		updateDirtyCCR();
		return m_dspRegs.getSR(_accessType);
	}

	void JitOps::setSR(const JitReg32& _src)
	{
		m_ccrDirty = static_cast<CCRMask>(0);
		m_dspRegs.setSR(_src);
	}

	void JitOps::setSR(const DspValue& _src)
	{
		m_ccrDirty = static_cast<CCRMask>(0);
		m_dspRegs.setSR(_src);
	}

	void JitOps::getALU2signed(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		const auto temp = r64(_dst.get());

		m_dspRegs.getALU(temp, _aluIndex);
		m_asm.sal(temp, asmjit::Imm(8));
		m_asm.sar(temp, asmjit::Imm(56));
		m_asm.and_(temp, asmjit::Imm(0xffffff));
	}

	void JitOps::getSSH(DspValue& _dst) const
	{
		_dst.temp(DspValue::Temp24);
		m_dspRegs.getSS(r64(_dst.get()));
		m_asm.shr(r64(_dst.get()), asmjit::Imm(24));
//		m_asm.and_(r32(_dst.get()), asmjit::Imm(0x00ffffff));
		decSP();
	}

	void JitOps::getSSL(DspValue& _dst) const
	{
		_dst.temp(DspValue::Temp24);
		m_dspRegs.getSS(r64(_dst.get()));
		m_asm.and_(r64(_dst.get()), 0x00ffffff);
	}

	void JitOps::transferAluTo24(DspValue& _dst, TWord _alu)
	{
		if (!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		m_dspRegs.getALU(r64(_dst.get()), _alu);
		transferSaturation(r64(_dst.get()));
	}

	void JitOps::transfer24ToAlu(TWord _alu, const DspValue& _src) const
	{
		AluRef r(m_block, _alu, false, true);
		_src.convertTo56(r.get());
	}

	void JitOps::callDSPFunc(void(*_func)(DSP*, TWord)) const
	{
		FuncArg r0(m_block, 0);
		m_block.asm_().mov(r0, asmjit::Imm(&m_block.dsp()));
		m_block.stack().call(asmjit::func_as_ptr(_func));
	}

	void JitOps::callDSPFunc(void(*_func)(DSP*, TWord), TWord _arg) const
	{
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(r1, asmjit::Imm(_arg));
		callDSPFunc(_func);
	}

	void JitOps::callDSPFunc(void(*_func)(DSP*, TWord), const JitRegGP& _arg) const
	{
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(r1, _arg);
		callDSPFunc(_func);
	}
}
