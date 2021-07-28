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
		m_block.mem().mov(_dst, _src);
	}

	void JitDspRegs::store24(TReg24& _dst, const JitRegGP& _src) const
	{
		m_block.mem().mov(_dst, _src);
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
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspM0 + _agu), _src);
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
		m_block.mem().mov(_dst.r8(), m_dsp.regs().sc.var);
	}

	void JitDspRegs::setSC(const JitReg32& _src) const
	{
		m_block.mem().mov(m_dsp.regs().sc.var, _src.r8());
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
		m_asm.mov(getSR(Write), _src);
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

	void JitDspRegs::getSSH(const JitReg32& _dst) const
	{
		getSS(r64(_dst));
		m_asm.shr(r64(_dst), Imm(24));
		m_asm.and_(r64(_dst), Imm(0x00ffffff));
		decSP();
	}

	void JitDspRegs::setSSH(const JitReg32& _src) const
	{
		incSP();
		const RegGP temp(m_block);
		getSS(temp);
		m_asm.ror(temp, Imm(24));
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp, r64(_src));
		m_asm.rol(temp, Imm(24));
		setSS(temp);
	}

	void JitDspRegs::getSSL(const JitReg32& _dst) const
	{
		getSS(r64(_dst));
		m_asm.and_(r64(_dst), 0x00ffffff);
	}

	void JitDspRegs::setSSL(const JitReg32& _src) const
	{
		const RegGP temp(m_block);
		getSS(temp);
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp, r64(_src));
		setSS(temp);
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
		const auto* first = reinterpret_cast<const uint64_t*>(&m_dsp.regs().ss[0].var);

		const auto ssIndex = g_funcArgGPs[0];//		const RegGP ssIndex(m_block);
		getSP(r32(ssIndex));
		m_asm.and_(ssIndex, Imm(0xf));

		m_block.mem().ptrToReg(_dst, first);
		m_asm.mov(_dst, ptr(_dst, ssIndex, 3, 0, 8));
	}

	void JitDspRegs::setSS(const JitReg64& _src) const
	{
		const auto* first = reinterpret_cast<const uint64_t*>(&m_dsp.regs().ss[0].var);

		const auto ssIndex = g_funcArgGPs[0];
		getSP(r32(ssIndex));
		m_asm.and_(ssIndex, Imm(0xf));

		const RegGP addr(m_block);
		m_block.mem().ptrToReg(addr, first);
		m_asm.mov(ptr(addr, ssIndex, 3, 0, 8), _src);
	}

	void JitDspRegs::decSP() const
	{
		m_asm.dec(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sp.var)));
		m_asm.dec(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sc.var)));
	}

	void JitDspRegs::incSP() const
	{
		m_asm.inc(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sp.var)));
		m_asm.inc(m_block.mem().ptr(regReturnVal, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sc.var)));
	}

	void JitDspRegs::mask56(const JitRegGP& _alu) const
	{
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(_alu, Imm(8));	
		m_asm.shr(_alu, Imm(8));
	}

	void JitDspRegs::mask48(const JitRegGP& _alu) const
	{
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(r64(_alu), Imm(16));	
		m_asm.shr(r64(_alu), Imm(16));
	}

	void JitDspRegs::setPC(const JitRegGP& _pc)
	{
		m_block.setNextPC(_pc);
	}
}
