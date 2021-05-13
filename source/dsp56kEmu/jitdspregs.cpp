#include "jitdspregs.h"

#include "jitmem.h"
#include "jitblock.h"

#include "dsp.h"

using namespace asmjit;
using namespace x86;

namespace dsp56k
{
	JitDspRegs::JitDspRegs(JitBlock& _block): m_block(_block), m_asm(_block.asm_()), m_dsp(_block.dsp())
	{
	}

	JitDspRegs::~JitDspRegs()
	{
		storeDSPRegs();
	}

	void JitDspRegs::loadAGU(int _agu)
	{
		// TODO: we can do better by reordering the DSP registers in memory so that we can load one 128 bit word at once
		const auto xm = xmm(xmmR0 + _agu);
		auto& mem = m_block.mem();
		mem.mov(xm, m_dsp.regs().m[_agu]);
		m_asm.pslldq(xm, Imm(4));

		const RegXMM xmmTemp(m_block);
		
		mem.mov(xmmTemp.get(), m_dsp.regs().n[_agu]);
		m_asm.movss(xm, xmmTemp.get());
		m_asm.pslldq(xm, Imm(4));

		mem.mov(xmmTemp.get(), m_dsp.regs().r[_agu]);
		m_asm.movss(xm, xmmTemp.get());

		setLoaded(LoadedRegR0 + _agu);
	}

	void JitDspRegs::loadALU(int _alu)
	{
		auto& alu = _alu ? m_dsp.regs().b : m_dsp.regs().a;
		
		m_block.mem().mov(xmm(xmmA + _alu), alu);

		setLoaded(LoadedRegA + _alu);
	}

	void JitDspRegs::loadXY(int _xy)
	{
		auto& xy = _xy ? m_dsp.regs().y : m_dsp.regs().x;

		m_block.mem().mov(xmm(xmmX + _xy), xy);

		setLoaded(LoadedRegX + _xy);
	}

	void JitDspRegs::storeAGU(int _agu)
	{
		const auto xm = xmm(xmmR0 + _agu);
		auto& mem = m_block.mem();

		mem.mov(m_dsp.regs().r[_agu], xm);
		m_asm.psrldq(xm, Imm(4));

		mem.mov(m_dsp.regs().n[_agu], xm);
		m_asm.psrldq(xm, Imm(4));

		mem.mov(m_dsp.regs().m[_agu], xm);

		setUnloaded(LoadedRegR0 + _agu);
	}

	void JitDspRegs::storeALU(int _alu)
	{
		auto& alu = _alu ? m_dsp.regs().b : m_dsp.regs().a;

		m_block.mem().mov(alu, xmm(xmmA + _alu));

		setUnloaded(LoadedRegA + _alu);
	}

	void JitDspRegs::storeXY(int _xy)
	{
		auto& xy = _xy ? m_dsp.regs().x : m_dsp.regs().y;

		m_block.mem().mov(xy, xmm(xmmX + _xy));

		setUnloaded(LoadedRegX + _xy);
	}

	void JitDspRegs::load24(const Gp& _dst, TReg24& _src) const
	{
		m_block.mem().mov(_dst, _src);
	}

	void JitDspRegs::store24(TReg24& _dst, const Gp& _src) const
	{
		m_block.mem().mov(_dst, _src);
	}

	void JitDspRegs::setR(int _agu, const JitReg64& _src)
	{
		if (!isLoaded(LoadedRegR0 + _agu))
			loadAGU(_agu);

		const auto xm(xmm(xmmR0 + _agu));

		if(CpuInfo::host().hasFeature(Features::kSSE4_1))
		{
			m_asm.pinsrd(xm, _src, Imm(0));
		}
		else
		{
			const RegXMM xmmTemp(m_block);

			m_asm.movd(xmmTemp.get(), _src);
			m_asm.movss(xm, xmmTemp.get());
		}
	}

	JitReg JitDspRegs::getPC()
	{
		if(!isLoaded(LoadedRegPC))
		{
			load24(regPC, m_dsp.regs().pc);
			setLoaded(LoadedRegPC);
		}

		return regPC;
	}

	JitReg JitDspRegs::getSR()
	{
		if(!isLoaded(LoadedRegSR))
		{
			load24(regSR, m_dsp.regs().sr);
			setLoaded(LoadedRegSR);
		}

		return regSR;
	}

	JitReg JitDspRegs::getLC()
	{
		if(!isLoaded(LoadedRegLC))
		{
			load24(regLC, m_dsp.regs().lc);			
			setLoaded(LoadedRegLC);
		}

		return regLC;
	}

	JitReg JitDspRegs::getExtMemAddr()
	{
		if(!isLoaded(LoadedRegExtMem))
		{
			m_block.mem().mov(regExtMem, m_dsp.memory().getBridgedMemoryAddress());
			setLoaded(LoadedRegExtMem);			
		}

		return regExtMem;
	}

	void JitDspRegs::getALU(const Gp _dst, const int _alu)
	{
		if(!isLoaded(LoadedRegA + _alu))
			loadALU(_alu);

		m_asm.movq(_dst, xmm(xmmA + _alu));
	}

	void JitDspRegs::setALU(int _alu, Gp _src)
	{
		assert(isLoaded(LoadedRegA + _alu));
		m_asm.movq(xmm(xmmA + _alu), _src);
	}

	void JitDspRegs::getXY(Gp _dst, int _xy)
	{
		if(!isLoaded(LoadedRegX + _xy))
			loadXY(_xy);

		m_asm.movq(_dst, xmm(xmmX + _xy));
	}

	void JitDspRegs::getXY0(const JitReg& _dst, const uint32_t _aluIndex)
	{
		getXY(_dst, _aluIndex);
		m_asm.and_(_dst, Imm(0xffffff));
	}

	void JitDspRegs::getXY1(const JitReg& _dst, const uint32_t _aluIndex)
	{
		getXY(_dst, _aluIndex);
		m_asm.shr(_dst, Imm(24));
	}

	void JitDspRegs::getALU0(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst, _aluIndex);
		m_asm.and_(_dst, Imm(0xffffff));
	}

	void JitDspRegs::getALU1(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst, _aluIndex);
		m_asm.shr(_dst, Imm(24));
		m_asm.and_(_dst, Imm(0xffffff));
	}

	void JitDspRegs::getALU2(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst, _aluIndex);
		m_asm.shr(_dst, Imm(48));
		m_asm.and_(_dst, Imm(0xff));
	}

	void JitDspRegs::getEP(const JitReg32& _dst)
	{
		m_block.mem().mov(_dst, m_dsp.regs().ep);
	}

	void JitDspRegs::getVBA(const JitReg32& _dst)
	{
		m_block.mem().mov(_dst, m_dsp.regs().vba);
	}

	void JitDspRegs::getSC(const JitReg32& _dst)
	{
		m_block.mem().mov(_dst.r8(), m_dsp.regs().sc.var);
	}

	void JitDspRegs::getSZ(const JitReg32& _dst)
	{
		m_block.mem().mov(_dst, m_dsp.regs().sz);
	}

	void JitDspRegs::getSR(const JitReg32& _dst)
	{
		m_asm.mov(_dst, getSR());
	}

	void JitDspRegs::getOMR(const JitReg32& _dst)
	{
		m_block.mem().mov(_dst, m_dsp.regs().omr);
	}

	void JitDspRegs::getSP(const JitReg32& _dst)
	{
		m_block.mem().mov(_dst, m_dsp.regs().sp);
	}

	void JitDspRegs::getSSH(const JitReg32& _dst)
	{
		getSS(_dst.r64());
		m_asm.shr(_dst.r64(), Imm(24));
		m_asm.and_(_dst.r64(), Imm(0x00ffffff));
		decSP();
	}

	void JitDspRegs::getSSL(const JitReg32& _dst)
	{
		getSS(_dst.r64());
		m_asm.and_(_dst.r64(), 0x00ffffff);
	}

	void JitDspRegs::getLA(const JitReg32& _dst) const
	{
		m_block.mem().mov(_dst, m_dsp.regs().la);
	}

	void JitDspRegs::getLC(const JitReg32& _dst) const
	{
		m_block.mem().mov(_dst, m_dsp.regs().lc);
	}

	void JitDspRegs::getSS(const JitReg64& _dst)
	{
		auto* first = &m_dsp.regs().ss[0].var;

		const RegGP ssIndex(m_block);
		getSP(ssIndex.get().r32());
		m_asm.and_(ssIndex, Imm(0xf));

		m_block.mem().ptrToReg(_dst, first);
		m_asm.mov(_dst, ptr(_dst, ssIndex, 3, 0, 8));
	}

	void JitDspRegs::decSP() const
	{
		const RegGP temp(m_block);

		m_asm.dec(m_block.mem().ptr(temp, &m_dsp.regs().sp.var));
		m_asm.dec(m_block.mem().ptr(temp, &m_dsp.regs().sc.var));
	}

	void JitDspRegs::incSP() const
	{
		const RegGP temp(m_block);

		m_asm.inc(m_block.mem().ptr(temp, &m_dsp.regs().sp.var));
		m_asm.inc(m_block.mem().ptr(temp, &m_dsp.regs().sc.var));
	}

	void JitDspRegs::loadDSPRegs()
	{
		for(auto i=0; i<8; ++i)
			loadAGU(i);

		loadALU(0);
		loadALU(1);

		loadXY(0);
		loadXY(1);
	}

	void JitDspRegs::storeDSPRegs()
	{
		// TODO: we should skip all registers that have not been written but only read

		for(auto i=0; i<8; ++i)
		{
			if(isLoaded(LoadedRegR0 + i))
				storeAGU(i);
		}

		if(isLoaded(LoadedRegA))			storeALU(0);
		if(isLoaded(LoadedRegB))			storeALU(1);

		if(isLoaded(LoadedRegX))			storeXY(0);
		if(isLoaded(LoadedRegY))			storeXY(1);

		if(isLoaded(LoadedRegPC))			store24(m_dsp.regs().pc, regPC);
		if(isLoaded(LoadedRegSR))			store24(m_dsp.regs().sr, regSR);
		if(isLoaded(LoadedRegLC))			store24(m_dsp.regs().lc, regLC);
	}
}
