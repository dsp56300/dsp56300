#include "jitdspregpool.h"
#include "jitconfig.h"
#include "jitops.h"
#include "dsp.h"

namespace dsp56k
{
	void dspExecDefaultPreventInterrupt(DSP*);
	void dspExecNop(DSP*);

	void JitOps::aluSignextendTo64(const JitReg64& _dst, const JitReg64& _src) const
	{
		// Sign-extends an ACCUMULATOR to 64 bits. Distinct from signextend56to64(), which is the generic
		// 56-bit helper used on raw values: a left-aligned accumulator is already natively sign-correct,
		// a raw right-aligned 56-bit value is not.
		if constexpr (g_leftAlignedAlu)
		{
			if(_dst != _src)
				m_asm.mov(_dst, _src);
			return;
		}
		signextend56to64(_dst, _src);
	}

	void JitOps::aluExtendTo64(const JitRegGP& _reg) const
	{
		if constexpr (!g_leftAlignedAlu)
			m_asm.shl(r64(_reg), asmjit::Imm(8));
	}

	void JitOps::aluRestoreFrom64(const JitRegGP& _reg) const
	{
		if constexpr (g_leftAlignedAlu)
			aluClearLowByte(_reg);	// the right-aligned form discards the low bits via shr; keep discarding them
		else
			m_asm.shr(r64(_reg), asmjit::Imm(8));
	}

	void JitOps::aluClearLowByte(const JitRegGP& _reg) const
	{
		// Enforces the left-aligned invariant: the 8 LSBs must never carry information. This is the exact
		// counterpart of the `shr alu,8` that the right-aligned code uses to discard shifted-out bits.
#ifdef HAVE_ARM64
		m_asm.and_(r64(_reg), r64(_reg), asmjit::Imm(~static_cast<uint64_t>(0xff)));
#else
		// 0xFFFFFF00 as a sign-extended imm32 is 0xFFFFFFFFFFFFFF00, so this is a single instruction
		m_asm.and_(r64(_reg), asmjit::Imm(-0x100));	// signed so the encoding sign-extends to 0xFFFFFFFFFFFFFF00
#endif
	}

	void JitOps::aluToLeftAligned(const JitRegGP& _reg) const
	{
		m_asm.shl(r64(_reg), asmjit::Imm(8));
	}

	void JitOps::aluFromLeftAligned(const JitRegGP& _reg) const
	{
		// logical shift: yields the 56-bit value zero-extended, matching the TReg56 memory format
		m_asm.shr(r64(_reg), asmjit::Imm(8));
	}

	void JitOps::signextend56to64(const JitReg64& _dst, const JitReg64& _src) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_dst, _src, asmjit::Imm(0), asmjit::Imm(56));
#else
		if(_dst != _src)
		{
			if(JitEmitter::hasBMI2())
			{
				m_asm.rorx(_dst, _src, asmjit::Imm(64-8));
				m_asm.sar(_dst, asmjit::Imm(8));
				return;
			}

			m_asm.mov(_dst, _src);
		}
		m_asm.sal(_dst, asmjit::Imm(8));
		m_asm.sar(_dst, asmjit::Imm(8));
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

	void JitOps::signextend48to56(const JitReg64& _dst, const JitReg64& _src) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_dst, _src, asmjit::Imm(0), asmjit::Imm(48));
		m_asm.ubfx(_dst, _dst, asmjit::Imm(0), asmjit::Imm(56));
#else
		if(_dst == _src)
		{
			m_asm.sal(_dst, asmjit::Imm(16));
		}
		else
		{
			m_asm.rol(_dst, _src, 16);
		}
		m_asm.sar(_dst, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_dst, asmjit::Imm(8));
#endif
	}

	void JitOps::signextend24to56(const JitReg64& _reg) const
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(24));
		m_asm.ubfx(_reg, _reg, asmjit::Imm(0), asmjit::Imm(56));
#else
		m_asm.sal(r32(_reg), asmjit::Imm(8));
		m_asm.movsxd(r64(_reg), r32(_reg));
		m_asm.shr(_reg, asmjit::Imm(8));
#endif
	}

	void JitOps::signextend24to64(JitEmitter& _a, const JitReg64& _dst, const JitReg64& _src, const uint32_t _shift)
	{
		// _shift scales the result up by 2^_shift. Both code paths below already end in a
		// right shift, so the scale is folded into it and costs nothing: the multiplier wants
		// its operand pre-scaled and would otherwise pay a separate shl for it.
		assert(_shift < 24 && "shift would push the 24 bit value out of the sign extension");
#ifdef HAVE_ARM64
		_a.sbfx(_dst, _src, asmjit::Imm(0), asmjit::Imm(24));
		if (_shift)
			_a.lsl(_dst, _dst, asmjit::Imm(_shift));
#else
		if (_dst != _src)
		{
			if (JitEmitter::hasBMI2())
			{
				// The rotate keeps _src's upper bits below the value instead of discarding them,
				// and a scaled sar retains the lowest _shift of them. 24 bit registers are always
				// zero extended in their host register - the unsigned read path (getXY0/1 with
				// _signextend false) is a bare 32 bit mov and would be wrong otherwise - so those
				// bits are zero and the low bits of the result stay clean.
				_a.rorx(_dst, _src, asmjit::Imm(64 - 40));
				_a.sar(_dst, asmjit::Imm(40 - _shift));
				return;
			}
			_a.mov(r32(_dst), r32(_src));
		}
		_a.sal(_dst, asmjit::Imm(40));
		_a.sar(_dst, asmjit::Imm(40 - _shift));
#endif
	}

	void JitOps::signextend24to64(const JitReg64& _dst, const JitReg64& _src, const uint32_t _shift) const
	{
		signextend24to64(m_asm, _dst, _src, _shift);
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
		m_asm.and_(t, asmjit::Imm(0xaaaaaaaa));	m_asm.and_(x, asmjit::Imm(0x55555555));
		m_asm.shr(t, asmjit::Imm(1));			m_asm.shl(x, asmjit::Imm(1));
		m_asm.or_(x, t);

		m_asm.mov(t, x);
		m_asm.and_(t, asmjit::Imm(0xcccccccc));	m_asm.and_(x, asmjit::Imm(0x33333333));
		m_asm.shr(t, asmjit::Imm(2));			m_asm.shl(x, asmjit::Imm(2));
		m_asm.or_(x, t);

		m_asm.mov(t, x);
		m_asm.and_(t, asmjit::Imm(0xf0f0f0f0));	m_asm.and_(x, asmjit::Imm(0x0f0f0f0f));
		m_asm.shr(t, asmjit::Imm(4));			m_asm.shl(x, asmjit::Imm(4));
		m_asm.or_(x, t);

		m_asm.bswap(x);
		m_asm.shr(x, asmjit::Imm(8));
#endif
	}

	void JitOps::pushPCSR()
	{
		DspValue pc(m_block);

		// m_pushPCFromReg: a call at a DO loop's end pushes the loop-updated next PC, which
		// emitLoopEndBeforeBranch() has just computed into the PC register, not the constant
		// address after the call.
		if (m_pushPCFromReg || m_fastInterruptMode == FastInterruptMode::Static)
		{
			pc = m_block.dspRegPool().read(PoolReg::DspPC);
		}
		else if (m_fastInterruptMode == FastInterruptMode::Dynamic)
		{
			/*	A block below Vba_End is only a fast interrupt if the DSP is actually servicing
				one, in which case the address to push is the interrupted program's PC and it
				lives in the register. Dynamic mode exists precisely because a device may also
				run ordinary code in the vector region, reached by a plain jump or call - and
				there the return address is the instruction after this one, exactly as anywhere
				else. Static mode is the device asserting that this cannot happen, so it keeps
				taking the register unconditionally.
			*/
			pc.temp(DspValue::Temp24);
			m_asm.mov(r32(pc), asmjit::Imm(m_pcCurrentOp + m_opSize));
		
			const RegGP processingMode(m_block);
			getDspProcessingMode(r64(processingMode));
		
			const SkipLabel notFastInterrupt(m_asm);
			m_asm.cmp(processingMode, asmjit::Imm(DSP::ProcessingMode::FastInterrupt));
			m_asm.jnz(notFastInterrupt);
		
			m_asm.mov(r32(pc), r32(m_block.dspRegPool().get(PoolReg::DspPC, true, false)));
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
			DspValue sr(m_block, DspSR, false, true);
			getSSL(sr);
			setSR(sr);	// still needed to call this to mark CCR bits as non-dirty
		}
		popPC();
	}
	void JitOps::popPC()
	{
		DspValue pc(m_block, PoolReg::DspPC, false, true);
		getSSH(pc);
	}

	void JitOps::setDspProcessingMode(const uint32_t _mode) const
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
			m_block.mem().mov(reinterpret_cast<uint64_t*>(&m_block.dsp().m_interruptFunc), reinterpret_cast<uint64_t>(ptr));
		}
		else if(_mode == DSP::LongInterrupt)
		{
			const auto* ptr = asmjit::func_as_ptr(&dspExecNop);
			m_block.mem().mov(reinterpret_cast<uint64_t*>(&m_block.dsp().m_interruptFunc), reinterpret_cast<uint64_t>(ptr));
		}
		else
			assert(false && "support missing");
	}

	void JitOps::getDspProcessingMode(const JitRegGP& _dst) const
	{
		if constexpr (sizeof(m_block.dsp().m_processingMode) == sizeof(uint32_t))
			m_block.mem().mov(r64(_dst), reinterpret_cast<uint32_t&>(m_block.dsp().m_processingMode));
		else if constexpr (sizeof(m_block.dsp().m_processingMode) == sizeof(uint64_t))
			m_block.mem().mov(r32(_dst), reinterpret_cast<uint64_t&>(m_block.dsp().m_processingMode));
	}

	TWord JitOps::getOpWordB()
	{
		++m_opSize;
		assert(m_opSize == 2);
		return m_opWordB;
	}

	void JitOps::getOpWordB(DspValue& _dst)
	{
		_dst.set(getOpWordB(), DspValue::Immediate24);
	}

	void JitOps::getXY0(DspValue& _dst, const uint32_t _aluIndex, bool _signextend, const uint32_t _shift) const
	{
		if (!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		const auto src = m_block.dspRegPool().get(_aluIndex ? PoolReg::DspY0 : PoolReg::DspX0, true, false);

		if (_signextend)
		{
			signextend24to64(r64(_dst), r64(src), _shift);
		}
		else
		{
			m_asm.mov(r32(_dst), r32(src));
			if (_shift)
				m_asm.shl(r64(_dst), asmjit::Imm(_shift));
		}
	}

	void JitOps::getXY1(DspValue& _dst, const uint32_t _aluIndex, bool _signextend, const uint32_t _shift) const
	{
		if (!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

		const auto src = m_block.dspRegPool().get(_aluIndex ? PoolReg::DspY1 : PoolReg::DspX1, true, false);

		if (_signextend)
		{
			signextend24to64(r64(_dst), r64(src), _shift);
		}
		else
		{
			m_asm.mov(r32(_dst), r32(src));
			if (_shift)
				m_asm.shl(r64(_dst), asmjit::Imm(_shift));
		}
	}

	void JitOps::setXY0(const uint32_t _xy, const DspValue& _src)
	{
		m_block.dspRegPool().setXY0(_xy, _src);
	}

	void JitOps::setXY1(const uint32_t _xy, const DspValue& _src)
	{
		m_block.dspRegPool().setXY1(_xy, _src);
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
		m_ccrRead |= CCR_All;

		_dst.release();
		updateDirtyCCR();
		_dst.acquire();
#ifdef HAVE_ARM64
		m_asm.ubfx(r32(_dst), r32(m_dspRegs.getSR(JitDspRegs::Read)), asmjit::Imm(0), asmjit::Imm(8));
#else
		m_asm.movzx(r32(_dst), m_dspRegs.getSR(JitDspRegs::Read).r8());
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
#ifdef HAVE_ARM64
		m_asm.ubfx(_dst, _dst, asmjit::Imm(8), asmjit::Imm(8));
#else
		m_asm.shr(_dst, asmjit::Imm(8));
		m_asm.and_(_dst, asmjit::Imm(0xff));
#endif
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
#ifdef HAVE_ARM64
		m_asm.bfi(r32(r), r32(_src), asmjit::Imm(0), asmjit::Imm(8));
#else
		m_asm.and_(r32(r), asmjit::Imm(0xffff00));
		m_asm.or_(r32(r), r32(_src));
#endif
		m_block.dspRegPool().movDspReg(m_block.dsp().regs().omr, r);
	}

	void JitOps::getSR(DspValue& _dst)
	{
		m_ccrRead |= CCR_All;
		updateDirtyCCR();
		m_dspRegs.getSR(_dst);
	}

	void JitOps::setSR(const DspValue& _src)
	{
		m_ccrWritten |= CCR_All;
		m_ccrDirty = static_cast<CCRMask>(0);
		m_dspRegs.setSR(_src);
	}

	void JitOps::getALU2signed(DspValue& _dst, uint32_t _aluIndex) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		else
			assert(_dst.getBitCount() == 24);

#ifdef HAVE_ARM64
		AluRef alu(m_block, _aluIndex, true, false);
		m_asm.sbfx(r64(_dst), r64(alu), asmjit::Imm(48 + g_aluBitOffset), asmjit::Imm(8));
		m_asm.ubfx(r32(_dst), r32(_dst), asmjit::Imm(0), asmjit::Imm(24));
#else
		const auto temp = r64(_dst.get());

		AluRef alu(m_block, _aluIndex, true, false);
		if constexpr (g_leftAlignedAlu)
			m_asm.mov(temp, r64(alu));	// a2 already sits at the top when left-aligned
		else
			m_asm.rol(temp, alu, 8);
		m_asm.sar(temp, asmjit::Imm(56));
		m_asm.and_(temp, asmjit::Imm(0xffffff));
#endif
	}

	void JitOps::getSSH(DspValue& _dst) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		m_dspRegs.getSS(r64(_dst.get()));
		m_asm.shr(r64(_dst.get()), asmjit::Imm(24));
//		m_asm.and_(r32(_dst.get()), asmjit::Imm(0x00ffffff));
		decSP();
	}

	void JitOps::getSSL(DspValue& _dst) const
	{
		if(!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		m_dspRegs.getSS(r64(_dst.get()));
		m_asm.and_(r64(_dst.get()), 0x00ffffff);
	}

	void JitOps::transferAluTo24(DspValue& _dst, const TWord _alu)
	{
		if (!_dst.isRegValid())
			_dst.temp(DspValue::Temp24);
		if(m_block.getMode() && m_block.getMode()->testSR(SRB_SA))
		{
			m_asm.mov(r64(_dst.get()), r64(m_dspRegs.getALU(_alu)));
			m_asm.sar(r64(_dst.get()), asmjit::Imm(32 + g_aluBitOffset));
			m_asm.and_(r32(_dst.get()), asmjit::Imm(0x00ffffff));
		}
		else
			transferSaturation24(r64(_dst.get()), r64(m_dspRegs.getALU(_alu)));
	}

	void JitOps::transfer24ToAlu(TWord _alu, const DspValue& _src) const
	{
		AluRef r(m_block, _alu, false, true);
		if(m_block.getMode() && m_block.getMode()->testSR(SRB_SA))
		{
			_src.copyTo(r32(r.get()), 24);
			m_block.asm_().shl(r64(r.get()), asmjit::Imm(48));
			m_block.asm_().sar(r64(r.get()), asmjit::Imm(48));
			m_block.asm_().shl(r64(r.get()), asmjit::Imm(32 + g_aluBitOffset));
		}
		else
			_src.convertTo56(r.get());
	}

	void JitOps::callDSPFunc(void(*_func)(DSP*, TWord), const TWord _arg) const
	{
		const FuncArg r0(m_block, 0);
		const FuncArg r1(m_block, 1);

		m_block.mem().makeDspPtr(r0);
		m_block.asm_().mov(r32(r1), asmjit::Imm(_arg));

		m_block.stack().call(asmjit::func_as_ptr(_func));
	}

	void JitOps::callDSPFunc(void(*_func)(DSP*, TWord), const JitRegGP& _arg) const
	{
		const FuncArg r0(m_block, 0);
		const FuncArg r1(m_block, 1);

		m_block.mem().makeDspPtr(r0);
		m_block.asm_().mov(r1, _arg);

		m_block.stack().call(asmjit::func_as_ptr(_func));
	}
}
