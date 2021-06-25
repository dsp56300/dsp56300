#include "jitdspregs.h"

#include "jitmem.h"
#include "jitblock.h"

#include "dsp.h"

using namespace asmjit;
using namespace x86;

namespace dsp56k
{
	JitDspRegs::JitDspRegs(JitBlock& _block): m_block(_block), m_asm(_block.asm_()), m_dsp(_block.dsp()), m_AguMchanged()
	{
		m_AguMchanged.fill(0);
	}

	JitDspRegs::~JitDspRegs()
	{
	}

	void JitDspRegs::getR(const JitReg& _dst, const int _agu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspR0 + _agu));
	}

	void JitDspRegs::getN(const JitReg& _dst, const int _agu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspN0 + _agu));
	}

	void JitDspRegs::getM(const JitReg& _dst, const int _agu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspM0 + _agu));
	}

	void JitDspRegs::load24(const Gp& _dst, const TReg24& _src) const
	{
		m_block.mem().mov(_dst, _src);
	}

	void JitDspRegs::store24(TReg24& _dst, const Gp& _src) const
	{
		m_block.mem().mov(_dst, _src);
	}

	JitDspRegPool& JitDspRegs::pool() const
	{
		return m_block.dspRegPool();
	}

	void JitDspRegs::setR(int _agu, const JitReg& _src)
	{
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspR0 + _agu), _src);
	}

	void JitDspRegs::setN(int _agu, const JitReg& _src)
	{
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspN0 + _agu), _src);
	}

	void JitDspRegs::setM(int _agu, const JitReg& _src)
	{
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspM0 + _agu), _src);
		m_block.mem().mov(m_AguMchanged[_agu], _src.r32());
	}

	JitReg JitDspRegs::getSR(AccessType _type)
	{
		return pool().get(JitDspRegPool::DspSR, _type & Read, _type & Write);
	}

	void JitDspRegs::getALU(const JitReg& _dst, const int _alu)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA + _alu));
	}

	JitReg JitDspRegs::getALU(int _alu, AccessType _access)
	{
		assert((_access & Write) == 0);
		return pool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA + _alu), _access & Read, _access & Write);
	}

	void JitDspRegs::setALU(const int _alu, const JitReg& _src, const bool _needsMasking)
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
		m_asm.xor_(alu, alu);

		if(pool().isParallelOp() && !pool().isLocked(r))
			pool().lock(r);
	}

	void JitDspRegs::getXY(const JitReg& _dst, int _xy)
	{
		pool().read(_dst, static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy));
	}

	JitReg JitDspRegs::getXY(int _xy, AccessType _access)
	{
		return pool().get(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), _access & Read, _access & Write);
	}

	void JitDspRegs::getXY0(const JitReg& _dst, const uint32_t _aluIndex)
	{
		getXY(_dst, _aluIndex);
		m_asm.and_(_dst, Imm(0xffffff));
	}

	void JitDspRegs::getXY1(const JitReg& _dst, const uint32_t _aluIndex)
	{
		getXY(_dst.r64(), _aluIndex);
		m_asm.shr(_dst.r64(), Imm(24));
	}

	void JitDspRegs::getALU0(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst, _aluIndex);
		m_asm.and_(_dst, Imm(0xffffff));
	}

	void JitDspRegs::getALU1(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst.r64(), _aluIndex);
		m_asm.shr(_dst.r64(), Imm(24));
		m_asm.and_(_dst.r64(), Imm(0xffffff));
	}

	void JitDspRegs::getALU2signed(const JitReg& _dst, uint32_t _aluIndex)
	{
		const RegGP temp(m_block);
		getALU(temp, _aluIndex);
		m_asm.sal(temp, Imm(8));
		m_asm.sar(temp, Imm(56));
		m_asm.and_(temp, Imm(0xffffff));
		m_asm.mov(_dst.r32(), temp.get().r32());
	}

	void JitDspRegs::setXY(const uint32_t _xy, const JitReg& _src)
	{
		mask48(_src);
		pool().write(static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspX + _xy), _src);
	}

	void JitDspRegs::setXY0(const uint32_t _xy, const JitReg& _src)
	{
		const RegGP temp(m_block);
		getXY(temp, _xy);
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp, _src.r64());
		setXY(_xy, temp);
	}

	void JitDspRegs::setXY1(const uint32_t _xy, const JitReg& _src)
	{
		const RegGP shifted(m_block);
		m_asm.mov(shifted, _src);
		m_asm.shl(shifted, Imm(24));
		const RegGP temp(m_block);
		getXY(temp, _xy);
		m_asm.and_(temp, Imm(0xffffff));
		m_asm.or_(temp, shifted.get());
		setXY(_xy, temp);
	}

	void JitDspRegs::setALU0(const uint32_t _aluIndex, const JitReg& _src)
	{
		const RegGP maskedSource(m_block);
		m_asm.mov(maskedSource, _src);
		m_asm.and_(maskedSource, Imm(0xffffff));

		const RegGP temp(m_block);
		getALU(temp, _aluIndex);
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp.get(), maskedSource.get());
		setALU(_aluIndex, temp);
	}

	void JitDspRegs::setALU1(const uint32_t _aluIndex, const JitReg32& _src)
	{
		const RegGP maskedSource(m_block);
		m_asm.mov(maskedSource, _src);
		m_asm.and_(maskedSource, Imm(0xffffff));

		const RegGP temp(m_block);
		getALU(temp, _aluIndex);
		m_asm.ror(temp, Imm(24));
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp.get(), maskedSource.get());
		m_asm.rol(temp, Imm(24));
		setALU(_aluIndex, temp);
	}

	void JitDspRegs::setALU2(const uint32_t _aluIndex, const JitReg32& _src)
	{
		const RegGP maskedSource(m_block);
		m_asm.mov(maskedSource, _src);
		m_asm.and_(maskedSource, Imm(0xff));

		const RegGP temp(m_block);
		getALU(temp, _aluIndex);
		m_asm.ror(temp, Imm(48));
		m_asm.and_(temp.get(), Imm(0xffffffffffffff00));
		m_asm.or_(temp.get(), maskedSource.get());
		m_asm.rol(temp, Imm(48));
		setALU(_aluIndex, temp);
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
		m_asm.mov(_dst, getSR(Read).r32());
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
		getSS(_dst.r64());
		m_asm.shr(_dst.r64(), Imm(24));
		m_asm.and_(_dst.r64(), Imm(0x00ffffff));
		decSP();
	}

	void JitDspRegs::setSSH(const JitReg32& _src) const
	{
		incSP();
		const RegGP temp(m_block);
		getSS(temp);
		m_asm.ror(temp, Imm(24));
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp, _src.r64());
		m_asm.rol(temp, Imm(24));
		setSS(temp);
	}

	void JitDspRegs::getSSL(const JitReg32& _dst) const
	{
		getSS(_dst.r64());
		m_asm.and_(_dst.r64(), 0x00ffffff);
	}

	void JitDspRegs::setSSL(const JitReg32& _src) const
	{
		const RegGP temp(m_block);
		getSS(temp);
		m_asm.and_(temp, Imm(0xffffffffff000000));
		m_asm.or_(temp, _src.r64());
		setSS(temp);
	}

	JitReg JitDspRegs::getLA(AccessType _type)
	{
		return pool().get(JitDspRegPool::DspLA, _type & Read, _type & Write);
	}

	void JitDspRegs::getLA(const JitReg32& _dst)
	{
		return pool().read(_dst.r64(), JitDspRegPool::DspLA);
	}

	void JitDspRegs::setLA(const JitReg32& _src)
	{
		return pool().write(JitDspRegPool::DspLA, _src);
	}

	JitReg JitDspRegs::getLC(AccessType _type)
	{
		return pool().get(JitDspRegPool::DspLC, _type & Read, _type & Write);
	}

	void JitDspRegs::getLC(const JitReg32& _dst)
	{
		return pool().read(_dst.r64(), JitDspRegPool::DspLC);
	}

	void JitDspRegs::setLC(const JitReg32& _src)
	{
		return pool().write(JitDspRegPool::DspLC, _src);
	}

	void JitDspRegs::getSS(const JitReg64& _dst) const
	{
		const auto* first = reinterpret_cast<const uint64_t*>(&m_dsp.regs().ss[0].var);

		const auto ssIndex = regArg0;//		const RegGP ssIndex(m_block);
		getSP(ssIndex.r32());
		m_asm.and_(ssIndex, Imm(0xf));

		m_block.mem().ptrToReg(_dst, first);
		m_asm.mov(_dst, ptr(_dst, ssIndex, 3, 0, 8));
	}

	void JitDspRegs::setSS(const JitReg64& _src) const
	{
		const auto* first = reinterpret_cast<const uint64_t*>(&m_dsp.regs().ss[0].var);

		const auto ssIndex = regArg0;
		getSP(ssIndex.r32());
		m_asm.and_(ssIndex, Imm(0xf));

		const RegGP addr(m_block);
		m_block.mem().ptrToReg(addr, first);
		m_asm.mov(ptr(addr, ssIndex, 3, 0, 8), _src);
	}

	void JitDspRegs::decSP() const
	{
		const RegGP temp(m_block);

		m_asm.dec(m_block.mem().ptr(temp, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sp.var)));
		m_asm.dec(m_block.mem().ptr(temp, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sc.var)));
	}

	void JitDspRegs::incSP() const
	{
		const RegGP temp(m_block);

		m_asm.inc(m_block.mem().ptr(temp, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sp.var)));
		m_asm.inc(m_block.mem().ptr(temp, reinterpret_cast<const uint32_t*>(&m_dsp.regs().sc.var)));
	}

	void JitDspRegs::mask56(const JitReg& _alu) const
	{
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(_alu, Imm(8));	
		m_asm.shr(_alu, Imm(8));
	}

	void JitDspRegs::mask48(const JitReg& _alu) const
	{
		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shl(_alu.r64(), Imm(16));	
		m_asm.shr(_alu.r64(), Imm(16));
	}

	void JitDspRegs::setPC(const JitReg& _pc)
	{
		m_block.mem().mov(m_block.nextPC(), _pc);
	}

	void JitDspRegs::updateDspMRegisters()
	{
		for(auto i=0; i<m_AguMchanged.size(); ++i)
		{
			if(m_AguMchanged[i])
				m_dsp.set_m(i, m_dsp.regs().m[i].var);
		}
	}
}
