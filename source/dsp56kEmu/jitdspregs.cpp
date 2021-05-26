#include "jitdspregs.h"

#include "jitmem.h"
#include "jitblock.h"

#include "dsp.h"

#include "asmjit/x86/x86features.h"

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

	void JitDspRegs::getR(const JitReg& _dst, const int _agu)
	{
		if (!isLoaded(LoadedRegR0 + _agu))
			loadAGU(_agu);

		m_asm.movd(_dst, xmm(xmmR0 + _agu));
	}

	void JitDspRegs::getN(const JitReg& _dst, const int _agu)
	{
		if (!isLoaded(LoadedRegR0 + _agu))
			loadAGU(_agu);

		const auto xm(xmm(xmmR0 + _agu));

		if (CpuInfo::host().hasFeature(Features::kSSE4_1))
		{
			m_asm.pextrd(_dst, xm, Imm(1));
		}
		else
		{
			m_asm.pshufd(xm, xm, Imm(0xe1)); // swap lower two words to get N in word 0
			m_asm.movd(_dst, xm);
			m_asm.pshufd(xm, xm, Imm(0xe1)); // swap back
		}
	}

	void JitDspRegs::getM(const JitReg& _dst, const int _agu)
	{
		if (!isLoaded(LoadedRegR0 + _agu))
			loadAGU(_agu);

		const auto xm(xmm(xmmR0 + _agu));

		if (CpuInfo::host().hasFeature(Features::kSSE4_1))
		{
			m_asm.pextrd(_dst, xm, Imm(2));
		}
		else
		{
			m_asm.pshufd(xm, xm, Imm(0xc6)); // swap words 0 and 2 to ret M in word 0
			m_asm.movd(_dst, xm);
			m_asm.pshufd(xm, xm, Imm(0xc6)); // swap back
		}
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
		auto& xy = _xy ? m_dsp.regs().y : m_dsp.regs().x;

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

	bool JitDspRegs::isLoaded(const uint32_t _reg) const
	{
		return m_loadedRegs & (1 << _reg);
	}

	void JitDspRegs::setR(int _agu, const JitReg& _src)
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

	void JitDspRegs::setN(int _agu, const JitReg& _src)
	{
		if (!isLoaded(LoadedRegR0 + _agu))
			loadAGU(_agu);

		const auto xm(xmm(xmmR0 + _agu));

		if(CpuInfo::host().hasFeature(Features::kSSE4_1))
		{
			m_asm.pinsrd(xm, _src, Imm(1));
		}
		else
		{
			const RegXMM xmmTemp(m_block);

			m_asm.movd(xmmTemp.get(), _src);

			m_asm.pshufd(xm, xm, Imm(0xe1)); // swap lower two words to get N in word 0
			m_asm.movss(xm, xmmTemp.get());
			m_asm.pshufd(xm, xm, Imm(0xe1)); // swap back
		}
	}

		void JitDspRegs::setM(int _agu, const JitReg& _src)
	{
		if (!isLoaded(LoadedRegR0 + _agu))
			loadAGU(_agu);

		const auto xm(xmm(xmmR0 + _agu));

		if(CpuInfo::host().hasFeature(Features::kSSE4_1))
		{
			m_asm.pinsrd(xm, _src, Imm(2));
		}
		else
		{
			const RegXMM xmmTemp(m_block);

			m_asm.movd(xmmTemp.get(), _src);

			m_asm.pshufd(xm, xm, Imm(0xc6)); // swap words 0 and 2 to ret M in word 0
			m_asm.movss(xm, xmmTemp.get());
			m_asm.pshufd(xm, xm, Imm(0xc6)); // swap back
		}
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

	JitReg JitDspRegs::getExtMemAddr()
	{
		if(!isLoaded(LoadedRegExtMem))
		{
			m_block.mem().mov(regExtMem, m_dsp.memory().getBridgedMemoryAddress());
			setLoaded(LoadedRegExtMem);			
		}

		return regExtMem;
	}

	JitReg JitDspRegs::getLA()
	{
		if(!isLoaded(LoadedRegLA))
		{
			load24(regLA, m_dsp.regs().la);
			setLoaded(LoadedRegLA);
		}
		return regLA;
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

	void JitDspRegs::getALU(const JitReg& _dst, const int _alu)
	{
		if(!isLoaded(LoadedRegA + _alu))
			loadALU(_alu);

		m_asm.movq(_dst, xmm(xmmA + _alu));
	}

	void JitDspRegs::setALU(int _alu, const JitReg& _src)
	{
		assert(isLoaded(LoadedRegA + _alu));
		mask56(_src);
		m_asm.movq(xmm(xmmA + _alu), _src);
	}

	void JitDspRegs::getXY(const JitReg& _dst, int _xy)
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
		m_asm.shr(_dst.r64(), Imm(24));
	}

	void JitDspRegs::getALU0(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst, _aluIndex);
		m_asm.and_(_dst, Imm(0xffffff));
	}

	void JitDspRegs::getALU1(const JitReg& _dst, uint32_t _aluIndex)
	{
		getALU(_dst, _aluIndex);
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
		m_asm.movq(xmm(xmmX + _xy), _src);
		setLoaded(LoadedRegX + _xy);
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
		m_block.mem().mov(_dst, m_dsp.regs().ep);
	}

	void JitDspRegs::setEP(const JitReg32& _src) const
	{
		m_block.mem().mov(m_dsp.regs().ep, _src);
	}

	void JitDspRegs::getVBA(const JitReg32& _dst) const
	{
		m_block.mem().mov(_dst, m_dsp.regs().vba);
	}

	void JitDspRegs::setVBA(const JitReg32& _src) const
	{
		m_block.mem().mov(m_dsp.regs().vba, _src);
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
		m_block.mem().mov(_dst, m_dsp.regs().sz);
	}

	void JitDspRegs::setSZ(const JitReg32& _src) const
	{
		m_block.mem().mov(m_dsp.regs().sz, _src);
	}

	void JitDspRegs::getSR(const JitReg32& _dst)
	{
		m_asm.mov(_dst, getSR());
	}

	void JitDspRegs::setSR(const JitReg32& _src)
	{
		m_asm.mov(getSR(), _src);
	}

	void JitDspRegs::getOMR(const JitReg32& _dst) const
	{
		m_block.mem().mov(_dst, m_dsp.regs().omr);
	}

	void JitDspRegs::setOMR(const JitReg32& _src) const
	{
		m_block.mem().mov(m_dsp.regs().omr, _src);
	}

	void JitDspRegs::getSP(const JitReg32& _dst) const
	{
		m_block.mem().mov(_dst, m_dsp.regs().sp);
	}

	void JitDspRegs::setSP(const JitReg32& _src) const
	{
		m_block.mem().mov(m_dsp.regs().sp, _src);
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
		m_asm.or_(temp, _src);
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
		m_asm.or_(temp, _src);
		setSS(temp);
	}

	void JitDspRegs::getLA(const JitReg32& _dst)
	{
		m_asm.mov(_dst, getLA().r32());
	}

	void JitDspRegs::setLA(const JitReg32& _src)
	{
		m_asm.mov(regLA, _src);
		setLoaded(LoadedRegLA);
	}

	void JitDspRegs::getLC(const JitReg32& _dst)
	{
		m_asm.mov(_dst, getLC().r32());
	}

	void JitDspRegs::setLC(const JitReg32& _src)
	{
		m_asm.mov(regLC, _src);
		setLoaded(LoadedRegLC);
	}

	void JitDspRegs::getSS(const JitReg64& _dst) const
	{
		const auto* first = reinterpret_cast<const uint64_t*>(&m_dsp.regs().ss[0].var);

		const RegGP ssIndex(m_block);
		getSP(ssIndex.get().r32());
		m_asm.and_(ssIndex, Imm(0xf));

		m_block.mem().ptrToReg(_dst, first);
		m_asm.mov(_dst, ptr(_dst, ssIndex, 3, 0, 8));
	}

	void JitDspRegs::setSS(const JitReg64& _src) const
	{
		const auto* first = reinterpret_cast<const uint64_t*>(&m_dsp.regs().ss[0].var);

		const RegGP ssIndex(m_block);
		getSP(ssIndex.get().r32());
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

	void JitDspRegs::loadDSPRegs()
	{
		for(auto i=0; i<8; ++i)
		{
			if(!isLoaded(LoadedRegR0 + i))
				loadAGU(i);
		}

		if(!isLoaded(LoadedRegA))
			loadALU(0);
		if(!isLoaded(LoadedRegB))
			loadALU(1);

		if(!isLoaded(LoadedRegX))
			loadXY(0);
		if(!isLoaded(LoadedRegY))
			loadXY(1);

		getSR();
		getLA();
		getLC();
	}

	void JitDspRegs::storeDSPRegs()
	{
		for(auto i=0; i<8; ++i)
		{
			if(isLoaded(LoadedRegR0 + i))
				storeAGU(i);
		}

		if(isLoaded(LoadedRegA))			storeALU(0);
		if(isLoaded(LoadedRegB))			storeALU(1);

		if(isLoaded(LoadedRegX))			storeXY(0);
		if(isLoaded(LoadedRegY))			storeXY(1);

		if(isLoaded(LoadedRegSR))
		{
			store24(m_dsp.regs().sr, regSR);
			m_dsp.resetCCRCache();
		}

		if(isLoaded(LoadedRegLA))
			store24(m_dsp.regs().la, regLA);

		if(isLoaded(LoadedRegLC))
			store24(m_dsp.regs().lc, regLC);
	}
}
