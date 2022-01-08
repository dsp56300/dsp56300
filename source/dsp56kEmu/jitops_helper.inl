#pragma once

#include "jitops.h"

#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::signextend56to64(const JitReg64& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
	}

	void JitOps::signextend48to64(const JitReg64& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(16));
		m_asm.sar(_reg, asmjit::Imm(16));
	}

	void JitOps::signextend48to56(const JitReg64& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(16));
		m_asm.sar(_reg, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
	}

	void JitOps::signextend24to56(const JitReg64& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(40));
		m_asm.sar(_reg, asmjit::Imm(32));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
	}

	void JitOps::signextend24to64(const JitReg64& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(40));
		m_asm.sar(_reg, asmjit::Imm(40));
	}

	void JitOps::signextend24To32(const JitReg32& _reg) const
	{
		m_asm.shl(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
	}

	void JitOps::updateAddressRegister(const JitReg64& _r, const TWord _mmm, const TWord _rrr, bool _writeR/* = true*/, bool _returnPostR/* = false*/)
	{
		if(_mmm == 6)													/* 110         */
		{
			getOpWordB(_r);
			return;
		}

		if(_mmm == 4)													/* 100 (Rn)    */
		{
			m_block.regs().getR(_r, _rrr);
			return;
		}

		const AguRegM m(m_block, _rrr, true);

		if(_mmm == 7)													/* 111 -(Rn)   */
		{
			m_dspRegs.getR(_r, _rrr);
			updateAddressRegisterConst(r32(_r),-1, r32(m.get()));
			if(_writeR)
				m_block.regs().setR(_rrr, _r);
			return;
		}

		AguRegR r(m_block, _rrr, true);

		if(_mmm == 5)													/* 101 (Rn+Nn) */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			signextend24To32(r32(n.get()));
			m_asm.mov(_r, r.get());
			updateAddressRegister(r32(_r), r32(n.get()), r32(m.get()));
			return;
		}

		if(!_returnPostR)
		{
			m_asm.mov(_r, r.get());
			if(!_writeR)
				return;
		}

		JitReg32 r32_;

		if(!_writeR)
		{
			m_asm.mov(_r, r.get());
			r32_ = r32(_r);
		}
		else
			r32_ = r32(r);

		if(_mmm == 0)													/* 000 (Rn)-Nn */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			m_asm.neg(n);
			updateAddressRegister(r32_, r32(n.get()), r32(m.get()));
		}	
		if(_mmm == 1)													/* 001 (Rn)+Nn */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			signextend24To32(r32(n.get()));
			updateAddressRegister(r32_, r32(n.get()), r32(m.get()));
		}
		if(_mmm == 2)													/* 010 (Rn)-   */
		{
			updateAddressRegisterConst(r32_,-1, r32(m.get()));
		}
		if(_mmm == 3)													/* 011 (Rn)+   */
		{
			updateAddressRegisterConst(r32_,1, r32(m.get()));
		}

		if(_writeR)
		{
			r.write();

			if(_returnPostR)
				m_asm.mov(_r, r.get());
		}
	}

	inline void JitOps::updateAddressRegisterModulo(const JitReg32& r, const JitReg32& n, const JitReg32& m) const
	{
/*
		const int32_t p				= (r&moduloMask) + n;
		const int32_t mt			= m - p;
		r							+= n + ((p>>31) & modulo) - (((mt)>>31) & modulo);
 */

		const auto moduloMask = regReturnVal;

		// Compute p
		{
			/* modulo mask mm = m
			mm |= mm >> 1;
			mm |= mm >> 2;
			mm |= mm >> 4;
			mm |= mm >> 8;
			*/

			const ShiftReg shifter(m_block);
			m_asm.bsr(r32(shifter), m);								// returns index of MSB that is 1
			m_asm.mov(moduloMask, asmjit::Imm(2));
			m_asm.shl(moduloMask, shifter.get().r8());
			m_asm.dec(moduloMask);

			/*
			rOffset = r & moduloMask
			p = rOffset + n

			we store it in n as n is no longer needed now
			*/
			const auto& p64 = shifter;
			const auto p = r32(p64.get());
			m_asm.mov(p, r);
			m_asm.and_(p, r32(moduloMask));
			m_asm.add(r, n);		// Increment r by n here.
			m_asm.add(n, p);
		}

		// r += ((p>>31) & modulo) - (((mt-p)>>31) & modulo);
		const auto p = n;		// We hid p in n.
		const auto& modulo = m;	// and modulo is m+1
		const auto& mtMinusP64 = moduloMask;
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
		{
			const RegGP pc(m_block);

			if(m_fastInterrupt)
			{
				m_block.mem().mov(pc, m_block.dsp().regs().pc);
			}
			else
			{
				m_asm.mov(pc, asmjit::Imm(m_pcCurrentOp + m_opSize));
			}

			setSSH(r32(pc.get()));
		}

		setSSL(r32(m_dspRegs.getSR(JitDspRegs::Read)));
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
		m_asm.mov(_dst, m_dspRegs.getSR(JitDspRegs::Read));
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::getCOM(const JitReg64& _dst) const
	{
		m_block.mem().mov(_dst, m_block.dsp().regs().omr);
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::getEOM(const JitReg64& _dst) const
	{
		m_block.mem().mov(_dst, m_block.dsp().regs().omr);
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
		m_block.mem().mov(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xffff00));
		m_asm.or_(r, _src);
		m_block.mem().mov(m_block.dsp().regs().omr, r);
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

	void JitOps::getXY0(const JitRegGP& _dst, const uint32_t _aluIndex) const
	{
		m_dspRegs.getXY(_dst, _aluIndex);
		m_asm.and_(_dst, asmjit::Imm(0xffffff));
	}

	void JitOps::getXY1(const JitRegGP& _dst, const uint32_t _aluIndex) const
	{
		m_dspRegs.getXY(r64(_dst), _aluIndex);
		m_asm.shr(r64(_dst), asmjit::Imm(24));
	}

	void JitOps::getALU0(const JitRegGP& _dst, uint32_t _aluIndex) const
	{
		m_dspRegs.getALU(_dst, _aluIndex);
		m_asm.and_(_dst, asmjit::Imm(0xffffff));
	}

	void JitOps::getALU1(const JitRegGP& _dst, uint32_t _aluIndex) const
	{
		m_dspRegs.getALU(r64(_dst), _aluIndex);
		m_asm.shr(r64(_dst), asmjit::Imm(24));
		m_asm.and_(r64(_dst), asmjit::Imm(0xffffff));
	}

	void JitOps::getALU2signed(const JitRegGP& _dst, uint32_t _aluIndex) const
	{
		const auto temp = r64(_dst);
		m_dspRegs.getALU(temp, _aluIndex);
		m_asm.sal(temp, asmjit::Imm(8));
		m_asm.sar(temp, asmjit::Imm(56));
		m_asm.and_(temp, asmjit::Imm(0xffffff));
	}

	void JitOps::setXY0(const uint32_t _xy, const JitRegGP& _src)
	{
		const auto temp = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), true, true);
		m_asm.and_(temp, asmjit::Imm(0xffffffffff000000));
		m_asm.or_(temp, r64(_src));
	}

	void JitOps::setXY1(const uint32_t _xy, const JitRegGP& _src)
	{
		const RegGP shifted(m_block);

		m_asm.mov(shifted, r64(_src));
		m_asm.shl(shifted, asmjit::Imm(24));

		const auto temp = m_block.dspRegPool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), true, true);
		m_asm.and_(temp, asmjit::Imm(0xffffff));
		m_asm.or_(temp, shifted.get());
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
		m_asm.shl(r64(_src), asmjit::Imm(40));
		m_asm.sar(r64(_src), asmjit::Imm(8));
		m_asm.shr(r64(_src), asmjit::Imm(8));
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
		m_block.asm_().mov(g_funcArgGPs[0], asmjit::Imm(&m_block.dsp()));
		m_block.stack().call(asmjit::func_as_ptr(_func));
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord), TWord _arg) const
	{
		FuncArg r1(m_block, 1);

		m_block.asm_().mov(g_funcArgGPs[1], asmjit::Imm(_arg));
		callDSPFunc(_func);
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord), const JitRegGP& _arg) const
	{
		FuncArg r1(m_block, 1);

		m_block.asm_().mov(g_funcArgGPs[1], _arg);
		callDSPFunc(_func);
	}
}
