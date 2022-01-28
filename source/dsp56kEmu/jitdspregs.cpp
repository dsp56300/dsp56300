#include "jitdspregs.h"

#include "jitmem.h"
#include "jitblock.h"

#include "dsp.h"
#include "jithelper.h"
#include "jitemitter.h"

using namespace asmjit;

namespace dsp56k
{
	JitDspRegs::JitDspRegs(JitBlock& _block): m_block(_block), m_asm(_block.asm_()), m_dsp(_block.dsp())
	{
	}

	JitDspRegs::~JitDspRegs()
	{
	}

	void JitDspRegs::getR(const JitRegGP& _dst, const int _agu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspR0 + _agu));
	}

	void JitDspRegs::getN(const JitRegGP& _dst, const int _agu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspN0 + _agu));
	}

	void JitDspRegs::getM(const JitRegGP& _dst, const int _agu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspM0 + _agu));
	}

	void JitDspRegs::load24(const JitRegGP& _dst, const TReg24& _src) const
	{
		m_block.dspRegPool().movDspReg(_dst, _src);
	}

	void JitDspRegs::store24(TReg24& _dst, const JitRegGP& _src) const
	{
		m_block.dspRegPool().movDspReg(_dst, _src);
	}

	JitDspRegPool& JitDspRegs::pool() const
	{
		return m_block.dspRegPool();
	}

	void JitDspRegs::setR(int _agu, const JitRegGP& _src)
	{
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspR0 + _agu), _src);
	}

	void JitDspRegs::setN(int _agu, const JitRegGP& _src)
	{
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspN0 + _agu), _src);
	}

	void JitDspRegs::setM(int _agu, const JitRegGP& _src)
	{
		// reference impl at DSP::set_m

		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspM0 + _agu), _src);

		const AguRegMmod mMod(m_block, _agu, false, true);
		const AguRegMmask mMask(m_block, _agu, false, true);

		const auto mod = r32(mMod);
		const auto mask = r32(mMask);

		const asmjit::Label end = m_asm.newLabel();
		const asmjit::Label isLinear = m_asm.newLabel();
		const asmjit::Label isModuloZero = m_asm.newLabel();
		const asmjit::Label isModulo = m_asm.newLabel();

#ifdef HAVE_ARM64
		m_asm.mov(regReturnVal, Imm(0xffffff));
		m_asm.cmp(_src, r32(regReturnVal));
#else
		m_asm.cmp(_src, Imm(0xffffff));
#endif
		m_asm.jz(isLinear);

		m_asm.mov(mod, _src);

		m_asm.and_(mod, asmjit::Imm(0xffff));
		m_asm.test(mod);
		m_asm.jz(isModuloZero);

#ifdef HAVE_ARM64
		m_asm.mov(regReturnVal, Imm(0x007fff));
		m_asm.cmp(mod, r32(regReturnVal));
#else
		m_asm.cmp(mod, asmjit::Imm(0x007fff));
#endif

		m_asm.jle(isModulo);
		m_asm.jmp(end);

		// zero modulo
		m_asm.bind(isModuloZero);
		m_asm.clr(mod);
		m_asm.jmp(end);

		// modulo
		m_asm.bind(isModulo);

		const ShiftReg shifter(m_block);
		m_asm.bsr(r32(shifter), r32(_src));								// returns index of MSB that is 1
		m_asm.mov(mask, asmjit::Imm(2));

#ifdef HAVE_ARM64
		m_asm.shl(mask, r32(shifter.get()));
#else
		m_asm.shl(mask, shiftOperand(shifter));
#endif
		m_asm.dec(mask);

		m_asm.inc(mod);

		m_asm.jmp(end);

		// linear
		m_asm.bind(isLinear);
		m_asm.clr(mod);
		m_asm.mov(mask, asmjit::Imm(0xffffff));

		m_asm.bind(end);
	}

	JitRegGP JitDspRegs::getSR(AccessType _type)
	{
		return pool().get(JitDspRegPool::DspSR, _type & Read, _type & Write);
	}

	void JitDspRegs::getALU(const JitRegGP& _dst, const int _alu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA + _alu));
	}

	JitRegGP JitDspRegs::getALU(int _alu, AccessType _access)
	{
		assert((_access != (Read | Write)) && "unable to read & write to the same register");
		const auto baseReg = _access & Write ? JitDspRegPool::DspAwrite : JitDspRegPool::DspA;
		return pool().get(static_cast<JitDspRegPool::DspReg>(baseReg + _alu), _access & Read, _access & Write);
	}

	void JitDspRegs::setALU(const int _alu, const JitRegGP& _src, const bool _needsMasking)
	{
		const auto r = static_cast<JitDspRegPool::DspReg>((pool().isParallelOp() ? JitDspRegPool::DspAwrite : JitDspRegPool::DspA) + _alu);

		pool().write(r, _src);

		if(_needsMasking)
			mask56(pool().get(r, true, true));

		if(pool().isParallelOp() && !pool().isLocked(r))
			pool().lock(r);
	}

	void JitDspRegs::clrALU(const TWord _alu)
	{
		const auto r = static_cast<JitDspRegPool::DspReg>((pool().isParallelOp() ? JitDspRegPool::DspAwrite : JitDspRegPool::DspA) + _alu);
		const auto alu = pool().get(r, false, true);
		m_asm.clr(alu);

		if(pool().isParallelOp() && !pool().isLocked(r))
			pool().lock(r);
	}

	void JitDspRegs::getXY(const JitRegGP& _dst, int _xy)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy));
	}

	JitRegGP JitDspRegs::getXY(int _xy, AccessType _access)
	{
		return pool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), _access & Read, _access & Write);
	}

	void JitDspRegs::setXY(const uint32_t _xy, const JitRegGP& _src)
	{
		mask48(_src);
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), _src);
	}

	void JitDspRegs::getEP(const JitReg32& _dst) const
	{
		load24(_dst, m_dsp.regs().ep);
	}

	void JitDspRegs::setEP(const JitReg32& _src) const
	{
		store24(m_dsp.regs().ep, _src);
	}

	void JitDspRegs::getVBA(const JitReg32& _dst) const
	{
		load24(_dst, m_dsp.regs().vba);
	}

	void JitDspRegs::setVBA(const JitReg32& _src) const
	{
		store24(m_dsp.regs().vba, _src);
	}

	void JitDspRegs::getSC(const JitReg32& _dst) const
	{
		m_block.dspRegPool().movDspReg(_dst, m_dsp.regs().sc);
	}

	void JitDspRegs::setSC(const JitReg32& _src) const
	{
		m_block.dspRegPool().movDspReg(m_dsp.regs().sc, _src);
	}

	void JitDspRegs::getSZ(const JitReg32& _dst) const
	{
		load24(_dst, m_dsp.regs().sz);
	}

	void JitDspRegs::setSZ(const JitReg32& _src) const
	{
		store24(m_dsp.regs().sz, _src);
	}

	void JitDspRegs::getSR(const JitReg32& _dst)
	{
		m_asm.mov(_dst, r32(getSR(Read)));
	}

	void JitDspRegs::setSR(const JitReg32& _src)
	{
		m_asm.mov(r32(getSR(Write)), _src);
	}

	void JitDspRegs::getOMR(const JitReg32& _dst) const
	{
		load24(_dst, m_dsp.regs().omr);
	}

	void JitDspRegs::setOMR(const JitReg32& _src) const
	{
		store24(m_dsp.regs().omr, _src);
	}

	void JitDspRegs::getSP(const JitReg32& _dst) const
	{
		load24(_dst, m_dsp.regs().sp);
	}

	void JitDspRegs::setSP(const JitReg32& _src) const
	{
		store24(m_dsp.regs().sp, _src);
	}

	JitRegGP JitDspRegs::getLA(AccessType _type)
	{
		return pool().get(JitDspRegPool::DspLA, _type & Read, _type & Write);
	}

	void JitDspRegs::getLA(const JitReg32& _dst)
	{
		return pool().read(r64(_dst), JitDspRegPool::DspLA);
	}

	void JitDspRegs::setLA(const JitReg32& _src)
	{
		return pool().write(JitDspRegPool::DspLA, _src);
	}

	JitRegGP JitDspRegs::getLC(AccessType _type)
	{
		return pool().get(JitDspRegPool::DspLC, _type & Read, _type & Write);
	}

	void JitDspRegs::getLC(const JitReg32& _dst)
	{
		return pool().read(r64(_dst), JitDspRegPool::DspLC);
	}

	void JitDspRegs::setLC(const JitReg32& _src)
	{
		return pool().write(JitDspRegPool::DspLC, _src);
	}

	void JitDspRegs::getSS(const JitReg64& _dst) const
	{
		const auto ssIndex = regReturnVal;
		getSP(r32(ssIndex));

#ifdef HAVE_ARM64
		m_asm.and_(ssIndex, ssIndex, Imm(0xf));
		m_asm.add(_dst, regDspPtr, asmjit::Imm(offsetof(DSP::SRegs, ss)));
#else
		m_asm.and_(ssIndex, Imm(0xf));
		m_asm.lea(_dst, m_block.dspRegPool().makeDspPtr(m_dsp.regs().ss[0]));
#endif

		m_asm.move(_dst, Jitmem::makePtr(_dst, ssIndex, 3, 8));
	}

	void JitDspRegs::setSS(const JitReg64& _src) const
	{
		const auto ssIndex = regReturnVal;
		getSP(r32(ssIndex));

		const RegGP addr(m_block);

#ifdef HAVE_ARM64
		m_asm.and_(ssIndex, ssIndex, Imm(0xf));
		m_asm.add(addr, regDspPtr, asmjit::Imm(offsetof(DSP::SRegs, ss)));
#else
		m_asm.and_(ssIndex, Imm(0xf));
		m_asm.lea(addr, m_block.dspRegPool().makeDspPtr(m_dsp.regs().ss[0]));
#endif
		m_asm.mov(Jitmem::makePtr(addr, ssIndex, 3, 8), _src);
	}

	void JitDspRegs::modifySS(const std::function<void(const JitReg64&)>& _func, bool _read, bool _write) const
	{
		const auto ssIndex = regReturnVal;
		getSP(r32(ssIndex));

		const RegGP ptrReg(m_block);
#ifdef HAVE_ARM64
		m_asm.and_(ssIndex, ssIndex, Imm(0xf));
		m_asm.add(ptrReg, regDspPtr, asmjit::Imm(offsetof(DSP::SRegs, ss)));
#else
		m_asm.and_(ssIndex, Imm(0xf));
		m_asm.lea(ptrReg, m_block.dspRegPool().makeDspPtr(m_dsp.regs().ss[0]));
#endif
		if(_read && !_write)
		{
			m_asm.move(ptrReg, Jitmem::makePtr(ptrReg, ssIndex, 3, 8));
			_func(r64(ptrReg.get()));
		}
		else
		{
			const RegGP ss(m_block);
			if (_read)
				m_asm.move(ss, Jitmem::makePtr(ptrReg, ssIndex, 3, 8));
			_func(r64(ss.get()));
			if(_write)
				m_asm.mov(Jitmem::makePtr(ptrReg, ssIndex, 3, 8), ss);
		}
	}

	void JitDspRegs::mask56(const JitRegGP& _alu) const
	{
#ifdef HAVE_ARM64
		// we need to work around the fact that there is no AND with 64 bit immediate operand and also ubfx cannot work with bits >= 32
		m_asm.shl(r64(_alu), Imm(8));
		m_asm.shr(r64(_alu), Imm(8));
//		m_asm.ubfx(_alu, _alu, Imm(0), Imm(56));
#else
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(_alu, Imm(8));
		m_asm.shr(_alu, Imm(8));
#endif
	}

	void JitDspRegs::mask48(const JitRegGP& _alu) const
	{
#ifdef HAVE_ARM64
		// we need to work around the fact that there is no AND with 64 bit immediate operand and also ubfx cannot work with bits >= 32
		m_asm.shl(r64(_alu), Imm(16));
		m_asm.shr(r64(_alu), Imm(16));
//		m_asm.ubfx(_alu, _alu, Imm(0), Imm(48));
#else
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(r64(_alu), Imm(16));	
		m_asm.shr(r64(_alu), Imm(16));
#endif
	}

	void JitDspRegs::setPC(const JitRegGP& _pc)
	{
		m_block.setNextPC(_pc);
	}
}
