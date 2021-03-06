#pragma once

#include "jitops.h"

#include "asmjit/core/operand.h"

namespace dsp56k
{
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

	void JitOps::updateAddressRegister(const JitReg64& _dst, const TWord _mmm, const TWord _rrr, bool _writeR/* = true*/, bool _returnPostR/* = false*/)
	{
		if(_mmm == 6)													/* 110         */
		{
			if(_dst.isValid())
				getOpWordB(_dst);
			return;
		}

		if(_mmm == 4)													/* 100 (Rn)    */
		{
			if (_dst.isValid())
				m_block.regs().getR(_dst, _rrr);
			return;
		}

		const AguRegM m(m_block, _rrr, true);

		if(_mmm == 7)													/* 111 -(Rn)   */
		{
			m_dspRegs.getR(_dst, _rrr);
			updateAddressRegisterConst(r32(_dst),-1, r32(m.get()), _rrr);
			if(_writeR)
				m_block.regs().setR(_rrr, _dst);
			return;
		}

		AguRegR rRef(m_block, _rrr, true);

		if(_mmm == 5)													/* 101 (Rn+Nn) */
		{
			if (!_dst.isValid())
				return;

			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			signextend24To32(r32(n.get()));
			m_asm.mov(_dst, rRef.get());
			updateAddressRegister(r32(_dst), r32(n.get()), r32(m.get()), _rrr);
			return;
		}

		if(!_returnPostR)
		{
			if(_dst.isValid())
				m_asm.mov(_dst, rRef.get());
			if(!_writeR)
				return;
		}

		JitReg32 r;

		if(!_writeR)
		{
			if (!_dst.isValid())
				return;

			m_asm.mov(_dst, rRef.get());
			r = r32(_dst);
		}
		else
			r = r32(rRef);

		if(_mmm == 0)													/* 000 (Rn)-Nn */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			m_asm.neg(n);
			updateAddressRegister(r, r32(n.get()), r32(m.get()), _rrr);
		}	
		if(_mmm == 1)													/* 001 (Rn)+Nn */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			signextend24To32(r32(n.get()));
			updateAddressRegister(r, r32(n.get()), r32(m.get()), _rrr);
		}
		if(_mmm == 2)													/* 010 (Rn)-   */
		{
			updateAddressRegisterConst(r,-1, r32(m.get()), _rrr);
		}
		if(_mmm == 3)													/* 011 (Rn)+   */
		{
			updateAddressRegisterConst(r,1, r32(m.get()), _rrr);
		}

		if(_writeR)
		{
			rRef.write();

			if(_returnPostR && _dst.isValid())
				m_asm.mov(_dst, rRef.get());
		}
	}

	inline void JitOps::updateAddressRegisterModulo(const JitReg32& r, const JitReg32& n, const JitReg32& m, const JitReg32& mMask) const
	{
/*
		const int32_t p				= (r&moduloMask) + n;
		const int32_t mt			= m - p;
		r							+= n + ((p>>31) & modulo) - (((mt)>>31) & modulo);
 */

		// Compute p
		{
			const ShiftReg shifter(m_block);

			/*
			rOffset = r & moduloMask
			p = rOffset + n

			we store it in n as n is no longer needed now
			*/
			const auto& p64 = shifter;
			const auto p = r32(p64.get());
			m_asm.mov(p, r);
			m_asm.and_(p, r32(mMask));
			m_asm.add(r, n);		// Increment r by n here.
			m_asm.add(n, p);
		}

		// r += ((p>>31) & modulo) - (((mt-p)>>31) & modulo);
		const auto p = n;		// We hid p in n.
		const auto& modulo = m;	// and modulo is m+1
		const auto& mtMinusP64 = regReturnVal;
		const auto mtMinusP = r32(mtMinusP64);

		m_asm.mov(mtMinusP, m);
		m_asm.sub(mtMinusP, p);
		m_asm.sar(mtMinusP, asmjit::Imm(31));
		m_asm.inc(modulo);
		m_asm.and_(mtMinusP, modulo);

		m_asm.sar(p, asmjit::Imm(31));
		m_asm.and_(p, modulo);
		m_asm.dec(modulo);

		m_asm.add(r, p);
		m_asm.sub(r, mtMinusP);
	}

	inline void JitOps::updateAddressRegisterMultipleWrapModulo(const JitReg32& _r, const JitReg32& _n,	const JitReg32& _m)
	{
	}

	inline void JitOps::updateAddressRegisterBitreverse(const JitReg32& _r, const JitReg32& _n, const JitReg32& _m)
	{
	}

	inline void JitOps::signed24To56(const JitReg64& _r) const
	{
		m_asm.shl(_r, asmjit::Imm(40));
		m_asm.sar(_r, asmjit::Imm(8));		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_r, asmjit::Imm(8));
	}

	void JitOps::pushPCSR()
	{
		const RegGP pc(m_block);

		if (m_fastInterrupt)
			m_block.dspRegPool().movDspReg(pc, m_block.dsp().regs().pc);
		else
			m_asm.mov(pc, asmjit::Imm(m_pcCurrentOp + m_opSize));

		setSSHSSL(r32(pc.get()), r32(getSR(JitDspRegs::Read)));
	}
	void JitOps::popPCSR()
	{
		{
			const RegGP sr(m_block);
			getSSL(r32(sr.get()));
			setSR(r32(sr.get()));
		}
		popPC();
	}
	void JitOps::popPC()
	{
		RegGP pc(m_block);
		getSSH(r32(pc.get()));
		m_dspRegs.setPC(pc);
	}

	void JitOps::setDspProcessingMode(uint32_t _mode)
	{
		const RegGP r(m_block);
		m_asm.mov(r32(r.get()), asmjit::Imm(_mode));

		if constexpr (sizeof(m_block.dsp().m_processingMode) == sizeof(uint32_t))
			m_block.mem().mov(reinterpret_cast<uint32_t&>(m_block.dsp().m_processingMode), r);
		else if constexpr (sizeof(m_block.dsp().m_processingMode) == sizeof(uint64_t))
			m_block.mem().mov(reinterpret_cast<uint64_t&>(m_block.dsp().m_processingMode), r);
	}

	inline TWord JitOps::getOpWordB()
	{
		++m_opSize;
		assert(m_opSize == 2);
		return m_opWordB;
	}

	inline void JitOps::getOpWordB(const JitRegGP& _dst)
	{
		m_asm.mov(r32(_dst), asmjit::Imm(getOpWordB()));
	}

	void JitOps::getMR(const JitReg64& _dst) const
	{
		m_asm.mov(_dst, m_dspRegs.getSR(JitDspRegs::Read));
		m_asm.shr(_dst, asmjit::Imm(8));
		m_asm.and_(_dst, asmjit::Imm(0xff));
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

	void JitOps::setCCR(const JitReg64& _src)
	{
		m_ccrDirty = static_cast<CCRMask>(0);
		m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(0xffff00));
		m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), _src);
	}

	void JitOps::setCOM(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_block.dspRegPool().movDspReg(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xffff00));
		m_asm.or_(r, _src);
		m_block.dspRegPool().movDspReg(m_block.dsp().regs().omr, r);
	}

	void JitOps::getSR(const JitReg32& _dst)
	{
		updateDirtyCCR();
		m_dspRegs.getSR(_dst);
	}

	inline JitRegGP JitOps::getSR(JitDspRegs::AccessType _accessType)
	{
		updateDirtyCCR();
		return m_dspRegs.getSR(_accessType);
	}

	void JitOps::setSR(const JitReg32& _src)
	{
		m_ccrDirty = static_cast<CCRMask>(0);
		m_dspRegs.setSR(_src);
	}

	void JitOps::getALU2signed(const JitRegGP& _dst, uint32_t _aluIndex) const
	{
		const auto temp = r64(_dst);
		m_dspRegs.getALU(temp, _aluIndex);
		m_asm.sal(temp, asmjit::Imm(8));
		m_asm.sar(temp, asmjit::Imm(56));
		m_asm.and_(temp, asmjit::Imm(0xffffff));
	}

	void JitOps::getSSH(const JitReg32& _dst) const
	{
		m_dspRegs.getSS(r64(_dst));
		m_asm.shr(r64(_dst), asmjit::Imm(24));
		m_asm.and_(r64(_dst), asmjit::Imm(0x00ffffff));
		decSP();
	}

	void JitOps::getSSL(const JitReg32& _dst) const
	{
		m_dspRegs.getSS(r64(_dst));
		m_asm.and_(r64(_dst), 0x00ffffff);
	}

	void JitOps::transferAluTo24(const JitRegGP& _dst, int _alu)
	{
		m_dspRegs.getALU(r64(_dst), _alu);
		transferSaturation(r64(_dst));
	}

	void JitOps::transfer24ToAlu(int _alu, const JitRegGP& _src)
	{
#ifdef HAVE_ARM64
		m_asm.sbfx(r32(_src), r32(_src), asmjit::Imm(0), asmjit::Imm(24));
		m_asm.lsl(r64(_src), r64(_src), asmjit::Imm(24));
#else
		m_asm.shl(r64(_src), asmjit::Imm(40));
		m_asm.sar(r64(_src), asmjit::Imm(8));
		m_asm.shr(r64(_src), asmjit::Imm(8));
#endif
		m_dspRegs.setALU(_alu, r64(_src), false);
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_bbbbb, Field_S>()>::type*> void JitOps::bitTestMemory(const TWord _op, const ExpectedBitValue _bitValue, const asmjit::Label _skip)
	{
		const RegGP r(m_block);
		readMem<Inst>(r, _op);

		bitTest<Inst>(_op, r, _bitValue, _skip);
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord)) const
	{
		FuncArg r0(m_block, 0);
		m_block.asm_().mov(r0, asmjit::Imm(&m_block.dsp()));
		m_block.stack().call(asmjit::func_as_ptr(_func));
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord), TWord _arg) const
	{
		FuncArg r1(m_block, 1);

		m_block.asm_().mov(r1, asmjit::Imm(_arg));
		callDSPFunc(_func);
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord), const JitRegGP& _arg) const
	{
		FuncArg r1(m_block, 1);

		m_block.asm_().mov(r1, _arg);
		callDSPFunc(_func);
	}
}
