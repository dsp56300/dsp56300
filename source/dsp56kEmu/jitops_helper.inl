#pragma once

#include "jitops.h"

#include "asmjit/core/operand.h"
#include "asmjit/x86/x86operand.h"

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

		const RegGP m(m_block);
		m_block.regs().getM(m, _rrr);

		if(_mmm == 7)													/* 111 -(Rn)   */
		{
			const RegGP n(m_block);
			m_asm.mov(n.get().r32(), asmjit::Imm(-1));
			m_dspRegs.getR(_r, _rrr);
			updateAddressRegister(_r.r32(),n.get().r32(),m.get().r32());
			if(_writeR)
				m_block.regs().setR(_rrr, _r);
			return;
		}

		m_dspRegs.getR(_r, _rrr);

		if(_mmm == 5)													/* 101 (Rn+Nn) */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			signextend24To32(n.get().r32());
			updateAddressRegister(_r.r32(),n.get().r32(), m.get().r32());
			return;
		}

		if(!_returnPostR)
			m_asm.push(_r);

		if(_mmm == 0)													/* 000 (Rn)-Nn */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			m_asm.neg(n);
			updateAddressRegister(_r.r32(),n.get().r32(),m.get().r32());
		}	
		if(_mmm == 1)													/* 001 (Rn)+Nn */
		{
			const RegGP n(m_block);
			m_dspRegs.getN(n, _rrr);
			signextend24To32(n.get().r32());
			updateAddressRegister(_r.r32(),n.get().r32(),m.get().r32());
		}
		if(_mmm == 2)													/* 010 (Rn)-   */
		{
			const RegGP n(m_block);
			m_asm.mov(n.get().r32(), asmjit::Imm(-1));
			updateAddressRegister(_r.r32(),n.get().r32(),m.get().r32());
		}
		if(_mmm == 3)													/* 011 (Rn)+   */
		{
			const RegGP n(m_block);
			m_asm.mov(n.get().r32(), asmjit::Imm(1));
			updateAddressRegister(_r.r32(),n.get().r32(),m.get().r32());
		}

		if(_writeR)
			m_block.regs().setR(_rrr, _r);

		if(!_returnPostR)
			m_asm.pop(_r);
	}

	inline void JitOps::updateAddressRegister(const JitReg32& _r, const JitReg32& _n, const JitReg32& _m)
	{
		const auto linear = m_asm.newLabel();
		const auto bitreverse = m_asm.newLabel();
		const auto modulo = m_asm.newLabel();
		const auto multipleWrapModulo = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		const PushGP n_sign(m_block, regReturnVal);	// sign extend n
		const PushGP n_abs(m_block, regExtMem);		// compare abs(n) with m

		const auto nAbs = n_abs.get().r32();

		m_asm.or_(_m.r16(), _m.r16());	// bit reverse
		m_asm.jz(bitreverse);

		m_asm.cmp(_m.r32(), asmjit::Imm(0xffffff));		// linear shortcut
		m_asm.jz(linear);
		
		m_asm.cmp(_m.r16(), asmjit::Imm(0x7fff));
		m_asm.jg(multipleWrapModulo);
		
		m_asm.mov(nAbs, _n);
		m_asm.neg(nAbs);
		m_asm.cmovl(nAbs, _n);

		m_asm.cmp(nAbs, _m);							// modulo or linear
		m_asm.jg(linear);

		// modulo:
		m_asm.bind(modulo);
		updateAddressRegisterModulo(_r.r32(), _n, _m.r32());
		m_asm.jmp(end);

		// multiple-wrap modulo:
		m_asm.bind(multipleWrapModulo);
		updateAddressRegisterMultipleWrapModulo(_r, _n, _m);
		m_asm.jmp(end);

		// bitreverse:
		m_asm.bind(bitreverse);
		updateAddressRegisterBitreverse(_r, _n, _m);
		m_asm.jmp(end);

		// linear:
		m_asm.bind(linear);
		m_asm.add(_r, _n);

		m_asm.bind(end);
		m_asm.and_(_r, asmjit::Imm(0xffffff));
	}

	inline void JitOps::updateAddressRegisterModulo(const JitReg32& r, const JitReg32& n, const JitReg32& m) const
	{
		/*
				const int32_t p				= (r&moduloMask) + n;
				const int32_t mt			= m - p;
				r							+= n + ((p>>31) & modulo) - (((mt)>>31) & modulo);
		 */

		const PushGP moduloMask(m_block, regLC);

		// Compute p
		{
			/* modulo mask mm = m
			mm |= mm >> 1;
			mm |= mm >> 2;
			mm |= mm >> 4;
			mm |= mm >> 8;
			*/

			const PushGP temp(m_block, asmjit::x86::rcx);
			m_asm.bsr(temp, m);								// returns index of MSB that is 1
			m_asm.mov(moduloMask, asmjit::Imm(1));
			m_asm.shl(moduloMask, temp.get());

			if(asmjit::CpuInfo::host().hasFeature(asmjit::x86::Features::kBMI))
			{
				m_asm.blsmsk(moduloMask, moduloMask);				
			}
			else
			{
				m_asm.mov(temp, moduloMask.get());
				m_asm.dec(moduloMask);
				m_asm.or_(moduloMask, temp.get());
			}

			/*
			rOffset = r & moduloMask
			p = rOffset + n

			we store it in n as n is no longer needed now
			*/
			const auto& p64 = temp;
			const auto p = p64.get().r32();
			m_asm.mov(p, r);
			m_asm.and_(p, moduloMask.get().r32());
			m_asm.add(r, n);		// Increment r by n here.
			m_asm.add(n, p);
		}

		// r += ((p>>31) & modulo) - (((mt-p)>>31) & modulo);
		const auto p = n;		// We hid p in n.
		const auto& modulo = m;	// and modulo is m+1
		const auto& mtMinusP64 = moduloMask;
		const auto mtMinusP = mtMinusP64.get().r32();

		m_asm.mov(mtMinusP, m);
		m_asm.sub(mtMinusP, p);
		m_asm.sar(mtMinusP, asmjit::Imm(31));
		m_asm.inc(modulo);
		m_asm.and_(mtMinusP, modulo);

		m_asm.sar(p, asmjit::Imm(31));
		m_asm.and_(p, modulo);

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
			m_asm.mov(pc, asmjit::Imm(m_pcCurrentOp + m_opSize));
			m_dspRegs.setSSH(pc.get().r32());
		}

		m_dspRegs.setSSL(m_dspRegs.getSR(JitDspRegs::Read).r32());
	}
	void JitOps::popPCSR()
	{
		{
			const RegGP sr(m_block);
			m_dspRegs.getSSL(sr.get().r32());
			setSR(sr.get().r32());
		}
		popPC();
	}
	void JitOps::popPC()
	{
		RegGP pc(m_block);
		m_dspRegs.getSSH(pc.get().r32());
		m_dspRegs.setPC(pc);
	}

	inline TWord JitOps::getOpWordB()
	{
		++m_opSize;
		assert(m_opSize == 2);
		return m_opWordB;
	}

	inline void JitOps::getOpWordB(const JitReg& _dst)
	{
		m_asm.mov(_dst.r32(), asmjit::Imm(getOpWordB()));
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

	void JitOps::setCCR(const JitReg64& _src)
	{
		m_ccrDirty = false;
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

	void JitOps::setEOM(const JitReg64& _src) const
	{
		const RegGP r(m_block);
		m_block.mem().mov(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xff00ff));
		m_asm.shl(_src, asmjit::Imm(8));
		m_asm.or_(r, _src);
		m_block.mem().mov(m_block.dsp().regs().omr, r);
		m_asm.shr(_src, asmjit::Imm(8));	// TODO: we don't wanna be destructive to the input for now
	}

	void JitOps::getSR(const JitReg32& _dst)
	{
		updateDirtyCCR();
		m_dspRegs.getSR(_dst);
	}

	inline JitReg JitOps::getSR(JitDspRegs::AccessType _accessType)
	{
		updateDirtyCCR();
		return m_dspRegs.getSR(_accessType);
	}

	void JitOps::setSR(const JitReg32& _src)
	{
		m_ccrDirty = false;
		m_dspRegs.setSR(_src);
	}

	void JitOps::transferAluTo24(const JitReg& _dst, int _alu)
	{
		m_dspRegs.getALU(_dst.r64(), _alu);
		transferSaturation(_dst.r64());
	}

	void JitOps::transfer24ToAlu(int _alu, const JitReg& _src)
	{
		m_asm.shl(_src.r64(), asmjit::Imm(40));
		m_asm.sar(_src.r64(), asmjit::Imm(8));
		m_asm.shr(_src.r64(), asmjit::Imm(8));
		m_dspRegs.setALU(_alu, _src.r64(), false);
	}

	void JitOps::transferSaturation(const JitReg& _dst)
	{
		// scaling

		/*
		if( sr_test_noCache(SR_S1) )
			_scale.var <<= 1;
		else if( sr_test_noCache(SR_S0) )
			_scale.var >>= 1;
		*/

		{
			const PushGP s0s1(m_block, asmjit::x86::rcx);
			m_asm.xor_(s0s1, s0s1.get());

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_S1));
			m_asm.setc(s0s1);
			m_asm.shl(_dst, s0s1.get());

			m_asm.bt(m_dspRegs.getSR(JitDspRegs::Read), asmjit::Imm(SRB_S0));
			m_asm.setc(s0s1);
			m_asm.shr(_dst, s0s1.get());
		}

		// saturated transfer
		/*
		const int64_t& test = _src.signextend<int64_t>();

		if( test < -140737488355328 )			// ff ff 800000 000000
		{
			sr_set( SR_L );
			_dst = 0x800000;
		}
		else if( test > 140737471578112 )		// 00 00 7fffff 000000
		{
			sr_set( SR_L );
			_dst = 0x7FFFFF;
		}
		else
			_dst = static_cast<int>(_src.var >> 24) & 0xffffff;
		*/
		{
			const RegGP tester(m_block);
			m_asm.mov(tester, _dst);
			signextend56to64(tester);

			// non-limited default
			m_asm.shr(_dst, asmjit::Imm(24));
			m_asm.and_(_dst, asmjit::Imm(0x00ffffff));

			// lower limit
			{
				const auto limit = regReturnVal;
				m_asm.mov(limit, asmjit::Imm(0xffff800000000000));
				m_asm.cmp(tester, limit);
			}
			{
				const auto minmax = regReturnVal;
				m_asm.mov(minmax, 0x800000);
				m_asm.cmovl(_dst, minmax);
			}
			ccr_update_ifLess(CCRB_L);

			// upper limit
			{
				const auto limit = regReturnVal;
				m_asm.mov(limit, asmjit::Imm(0x00007fffff000000));
				m_asm.cmp(tester, limit);
			}
			{
				const auto minmax = regReturnVal;
				m_asm.mov(minmax, 0x7fffff);
				m_asm.cmovg(_dst, minmax);
			}
		}
		ccr_update_ifGreater(CCRB_L);
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_bbbbb, Field_S>()>::type*> void JitOps::bitTestMemory(const TWord _op, const ExpectedBitValue _bitValue, const asmjit::Label _skip)
	{
		const RegGP r(m_block);
		readMem<Inst>(r, _op);

		bitTest<Inst>(_op, r, _bitValue, _skip);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_bbbbb>()>::type*> void JitOps::bitTest(TWord op, const JitReg& _value, const ExpectedBitValue _bitValue, const asmjit::Label _skip) const
	{
		const auto bit = getBit<Inst>(op);
		m_asm.bt(_value, asmjit::Imm(bit));

		if(_bitValue == BitSet)
			m_asm.jnc(_skip);
		else if(_bitValue == BitClear)
			m_asm.jc(_skip);
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord)) const
	{
		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));

		{
			PushBeforeFunctionCall backup(m_block);
			m_block.asm_().call(asmjit::func_as_ptr(_func));
		}
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord), TWord _arg) const
	{
		PushGP r1(m_block, regArg1);
//		PushGP rPadding(m_block, regArg2);

		m_block.asm_().mov(regArg1, asmjit::Imm(_arg));
		callDSPFunc(_func);
	}

	void JitOps::callDSPFunc(void(* _func)(DSP*, TWord), const JitReg& _arg) const
	{
		PushGP r1(m_block, regArg1);
//		PushGP rPadding(m_block, regArg2);

		m_block.asm_().mov(regArg1, _arg);
		callDSPFunc(_func);
	}
}
