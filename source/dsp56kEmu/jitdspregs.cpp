#include "jitdspregs.h"

#include "jitmem.h"
#include "jitblock.h"

#include "dsp.h"
#include "jithelper.h"
#include "jitemitter.h"

#include "agu.h"

using namespace asmjit;

namespace dsp56k
{
	JitDspRegs::JitDspRegs(JitBlock& _block): m_block(_block), m_asm(_block.asm_()), m_dsp(_block.dsp())
	{
	}

	JitDspRegs::~JitDspRegs() = default;

	DspValue JitDspRegs::getR(const TWord _agu) const
	{
		return DspValue(m_block, PoolReg::DspR0, true, false, _agu);
	}

	void JitDspRegs::getR(DspValue& _dst, const TWord _agu) const
	{
		_dst = DspValue(m_block, static_cast<PoolReg>(PoolReg::DspR0 + _agu), true, false);
	}

	void JitDspRegs::getN(DspValue& _dst, const TWord _agu) const
	{
		_dst = DspValue(m_block, static_cast<PoolReg>(PoolReg::DspN0 + _agu), true, false);
	}

	void JitDspRegs::getM(DspValue& _dst, const TWord _agu) const
	{
		_dst = DspValue(m_block, static_cast<PoolReg>(PoolReg::DspM0 + _agu), true, false);
	}

	void JitDspRegs::load24(DspValue& _dst, const TReg24& _src) const
	{
		m_block.dspRegPool().movDspReg(_dst, _src);
	}

	void JitDspRegs::load24(const JitRegGP& _dst, const TReg24& _src) const
	{
		m_block.dspRegPool().movDspReg(_dst, _src);
	}

	void JitDspRegs::store24(TReg24& _dst, const JitRegGP& _src) const
	{
		m_block.dspRegPool().movDspReg(_dst, _src);
	}

	void JitDspRegs::store24(TReg24& _dst, const DspValue& _src) const
	{
		m_block.dspRegPool().movDspReg(_dst, _src);
	}

	void JitDspRegs::reset()
	{
		m_ccrDirtyFlags = static_cast<CCRMask>(0);
	}

	JitDspRegPool& JitDspRegs::pool() const
	{
		return m_block.dspRegPool();
	}

	void JitDspRegs::setR(TWord _agu, const DspValue& _src) const
	{
		pool().write(static_cast<PoolReg>(PoolReg::DspR0 + _agu), _src);
	}

	void JitDspRegs::setN(TWord _agu, const DspValue& _src) const
	{
		pool().write(static_cast<PoolReg>(PoolReg::DspN0 + _agu), _src);
	}

	void JitDspRegs::setM(TWord _agu, const DspValue& _src) const
	{
		pool().write(static_cast<PoolReg>(PoolReg::DspM0 + _agu), _src);

		const DspValue mMod = makeDspValueAguReg(m_block, PoolReg::DspM0mod, _agu, false, true);
		const DspValue mMask = makeDspValueAguReg(m_block, PoolReg::DspM0mask, _agu, false, true);

		const auto mod = r32(mMod);
		const auto mask = r32(mMask);

		if(_src.isImmediate())
		{
			const auto val = _src.imm24();

			if (val == 0xffffff)
			{
				m_asm.mov(mod, asmjit::Imm(0));
				m_asm.mov(mask, asmjit::Imm(0xffffff));
				return;
			}

			const TWord moduloTest = (val & 0xffff);

			if (moduloTest == 0)
			{
				m_asm.mov(mask, asmjit::Imm(0));
			}
			else if (moduloTest <= 0x007fff)
			{
				m_asm.mov(mask, asmjit::Imm(AGU::calcModuloMask(val)));
				m_asm.mov(mod, asmjit::Imm(val + 1));
			}
			else
			{
				const auto m = moduloTest & 0x3fff;
				if (AGU::calcModuloMask(m) != m)
					LOG("Configured multiple-wrap-around mode with non power-of-two size!" << HEX(m));
				m_asm.mov(mask, asmjit::Imm(m));
				m_asm.mov(mod, asmjit::Imm(-1));
			}
			return;
		}

		// if the source is an M reg too we can copy the previously calculated values from the source
		const auto dspReg = _src.getDspReg().dspReg();

		if(dspReg >= DspM0 && dspReg <= DspM7)
		{
			const auto m = dspReg - DspM0;

			m_asm.mov(mod , r32(m_block.dspRegPool().get(static_cast<PoolReg>(DspM0mod  + m), true, false)));
			m_asm.mov(mask, r32(m_block.dspRegPool().get(static_cast<PoolReg>(DspM0mask + m), true, false)));

			return;
		}

		const asmjit::Label end = m_asm.newLabel();
		const asmjit::Label isLinear = m_asm.newLabel();
		const asmjit::Label isBitreverse = m_asm.newLabel();
		const asmjit::Label isModulo = m_asm.newLabel();

#ifdef HAVE_ARM64
		{
			const RegScratch scratch(m_block);
			m_asm.mov(scratch, Imm(0xffffff));
			m_asm.cmp(_src.get(), r32(scratch));
		}
#else
		m_asm.cmp(_src.get(), Imm(0xffffff));
#endif
		m_asm.jz(isLinear);

		m_asm.mov(mod, _src.get());

		m_asm.and_(mod, asmjit::Imm(0xffff));
		m_asm.test_(mod);
		m_asm.jz(isBitreverse);

#ifdef HAVE_ARM64
		{
			const RegScratch scratch(m_block);
			m_asm.mov(scratch, Imm(0x007fff));
			m_asm.cmp(mod, r32(scratch));
		}
#else
		m_asm.cmp(mod, asmjit::Imm(0x007fff));
#endif

		m_asm.jle(isModulo);

		// multiple-wrap modulo
		m_asm.mov(r32(mask), mod);
		m_asm.and_(r32(mask), asmjit::Imm(0x3fff));
		m_asm.mov(r32(mod), asmjit::Imm(-1));
		m_asm.jmp(end);

		// zero modulo = bitreverse
		m_asm.bind(isBitreverse);
		m_asm.clr(mod);
		m_asm.jmp(end);

		// modulo
		m_asm.bind(isModulo);

		const ShiftReg shifter(m_block);
		m_asm.bsr(r32(shifter), r32(_src.get()));							// returns index of MSB that is 1
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

	JitRegGP JitDspRegs::getSR(AccessType _type) const
	{
		return pool().get(PoolReg::DspSR, _type & Read, _type & Write);
	}

	void JitDspRegs::getALU(const JitRegGP& _dst, const TWord _alu) const
	{
		pool().read(_dst, static_cast<PoolReg>(PoolReg::DspA + _alu));
	}

	DspValue JitDspRegs::getALU(TWord _alu) const
	{
		return pool().read(static_cast<PoolReg>(PoolReg::DspA + _alu));
	}

	void JitDspRegs::setALU(const TWord _alu, const DspValue& _src, const bool _needsMasking) const
	{
		const auto r = static_cast<PoolReg>((pool().isParallelOp() ? PoolReg::DspAwrite : PoolReg::DspA) + _alu);

		pool().write(r, _src);

		if(_needsMasking)
			mask56(pool().get(r, true, true));

		if(pool().isParallelOp() && !pool().isLocked(r))
			pool().lock(r);
	}

	void JitDspRegs::clrALU(const TWord _alu) const
	{
		const auto r = static_cast<PoolReg>((pool().isParallelOp() ? PoolReg::DspAwrite : PoolReg::DspA) + _alu);
		const auto alu = pool().get(r, false, true);
		m_asm.clr(alu);

		if(pool().isParallelOp() && !pool().isLocked(r))
			pool().lock(r);
	}

	void JitDspRegs::getXY(const JitRegGP& _dst, TWord _xy) const
	{
		pool().getXY(r64(_dst), _xy);
	}

	void JitDspRegs::getEP(DspValue& _dst) const
	{
		load24(_dst, m_dsp.regs().ep);
	}

	void JitDspRegs::setEP(const DspValue& _src) const
	{
		store24(m_dsp.regs().ep, _src);
	}

	void JitDspRegs::getVBA(DspValue& _dst) const
	{
		load24(_dst, m_dsp.regs().vba);
	}

	void JitDspRegs::setVBA(const DspValue& _src) const
	{
		store24(m_dsp.regs().vba, _src);
	}

	void JitDspRegs::getSC(DspValue& _dst) const
	{
		m_block.dspRegPool().movDspReg(_dst, m_dsp.regs().sc);
	}

	void JitDspRegs::setSC(const DspValue& _src) const
	{
		m_block.dspRegPool().movDspReg(m_dsp.regs().sc, _src);
	}

	void JitDspRegs::getSZ(DspValue& _dst) const
	{
		load24(_dst, m_dsp.regs().sz);
	}

	void JitDspRegs::setSZ(const DspValue& _src) const
	{
		store24(m_dsp.regs().sz, _src);
	}

	void JitDspRegs::getSR(DspValue& _dst) const
	{
		_dst = DspValue(m_block, PoolReg::DspSR, true, false);
	}

	void JitDspRegs::setSR(const DspValue& _src) const
	{
		pool().write(PoolReg::DspSR, _src);
	}

	void JitDspRegs::getOMR(DspValue& _dst) const
	{
		load24(_dst, m_dsp.regs().omr);
	}

	void JitDspRegs::setOMR(const DspValue& _src) const
	{
		store24(m_dsp.regs().omr, _src);
	}

	void JitDspRegs::getSP(const JitReg32& _dst) const
	{
		load24(_dst, m_dsp.regs().sp);
	}

	void JitDspRegs::getSP(DspValue& _dst) const
	{
		load24(_dst, m_dsp.regs().sp);
	}

	void JitDspRegs::setSP(const DspValue& _src) const
	{
		store24(m_dsp.regs().sp, _src);
	}

	JitRegGP JitDspRegs::getLA(AccessType _type) const
	{
		return pool().get(PoolReg::DspLA, _type & Read, _type & Write);
	}

	void JitDspRegs::getLA(DspValue& _dst) const
	{
		if(!_dst.isRegValid())
		{
			_dst = DspValue(m_block, PoolReg::DspLA, true, false);
		}
		else
		{
			pool().read(_dst.get(), PoolReg::DspLA);
		}
	}

	void JitDspRegs::setLA(const DspValue& _src) const
	{
		return pool().write(PoolReg::DspLA, _src);
	}

	JitRegGP JitDspRegs::getLC(AccessType _type) const
	{
		return pool().get(PoolReg::DspLC, _type & Read, _type & Write);
	}

	void JitDspRegs::getLC(DspValue& _dst) const
	{
		if(!_dst.isRegValid())
		{
			_dst = DspValue(m_block, PoolReg::DspLC, true, false);
		}
		else
		{
			pool().read(_dst.get(), PoolReg::DspLC);
		}
	}

	void JitDspRegs::setLC(const DspValue& _src) const
	{
		return pool().write(PoolReg::DspLC, _src);
	}

	void JitDspRegs::getSS(const JitReg64& _dst) const
	{
		const RegScratch ssIndex(m_block);
		getSP(r32(ssIndex));

		m_asm.and_(r32(ssIndex), Imm(0xf));
#ifdef HAVE_ARM64
		m_asm.add(_dst, regDspPtr, asmjit::Imm(offsetof(DSP::SRegs, ss)));
		m_asm.move(_dst, Jitmem::makePtr(_dst, ssIndex, 3, 8));
#else
		m_asm.move(_dst, ptr(regDspPtr, ssIndex, 3, offsetof(DSP::SRegs, ss), 8));
#endif
	}

	void JitDspRegs::setSS(const JitReg64& _src) const
	{
		const RegScratch ssIndex(m_block);
		getSP(r32(ssIndex));
		m_asm.and_(ssIndex, Imm(0xf));

#ifdef HAVE_ARM64
		const RegGP addr(m_block);
		m_asm.add(addr, regDspPtr, asmjit::Imm(offsetof(DSP::SRegs, ss)));
		m_asm.mov(Jitmem::makePtr(addr, ssIndex, 3, 8), _src);
#else
		m_asm.mov(ptr(regDspPtr, ssIndex, 3, offsetof(DSP::SRegs, ss), 8), _src);
#endif
	}

	void JitDspRegs::modifySS(const std::function<void(const JitReg64&)>& _func, bool _read, bool _write) const
	{
		const RegScratch ssIndex(m_block);
		getSP(r32(ssIndex));
		m_asm.and_(r32(ssIndex), Imm(0xf));

#ifdef HAVE_ARM64
		const RegGP ptrReg(m_block);

		m_asm.add(ptrReg, regDspPtr, asmjit::Imm(offsetof(DSP::SRegs, ss)));
		const auto ptr = Jitmem::makePtr(ptrReg, ssIndex, 3, 8);

		if(_read && !_write)
		{
			m_asm.move(ptrReg, ptr);
			_func(r64(ptrReg.get()));
		}
		else
		{
			const RegGP ss(m_block);
			if (_read)
				m_asm.move(ss, ptr);
			_func(r64(ss.get()));
			if(_write)
				m_asm.mov(ptr, ss);
		}
#else
			const auto ptr = asmjit::x86::ptr(regDspPtr, ssIndex, 3, offsetof(DSP::SRegs, ss), 8);

			const RegGP ss(m_block);
			if (_read)
				m_asm.move(ss, ptr);
			_func(r64(ss.get()));
			if(_write)
				m_asm.mov(ptr, ss);
#endif
	}

	void JitDspRegs::mask56(const JitRegGP& _alu) const
	{
#ifdef HAVE_ARM64
		m_asm.ubfx(r64(_alu), r64(_alu), Imm(0), Imm(56));
#else
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(_alu, Imm(8));
		m_asm.shr(_alu, Imm(8));
#endif
	}

	void JitDspRegs::mask48(const JitRegGP& _alu) const
	{
#ifdef HAVE_ARM64
		m_asm.ubfx(r64(_alu), r64(_alu), Imm(0), Imm(48));
#else
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(r64(_alu), Imm(16));	
		m_asm.shr(r64(_alu), Imm(16));
#endif
	}

	void JitDspRegs::setPC(const DspValue& _pc) const
	{
		m_block.setNextPC(_pc);
	}
}
