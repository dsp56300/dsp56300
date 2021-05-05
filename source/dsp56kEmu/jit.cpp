#include "jit.h"

#include "jitmem.h"

#include "dsp.h"

#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"

using namespace asmjit;
using namespace x86;

namespace dsp56k
{
	Jit::Jit(DSP& _dsp) : m_dsp(_dsp)
	{
		m_code.init(m_rt.environment());

		m_asm = new Assembler(&m_code);

		loadDSPRegs();

		getR(rax, 0);
		getN(rbx, 0);
		getM(rcx, 0);

		getR(Jitmem::ptr(*m_asm, m_dsp.regs().r[7]), 0);
		getN(Jitmem::ptr(*m_asm, m_dsp.regs().n[7]), 0);
		getM(Jitmem::ptr(*m_asm, m_dsp.regs().m[7]), 0);

		storeDSPRegs();

		m_asm->ret();

		typedef void (*Func)(TWord op);

		Func func;

		const auto err = m_rt.add(&func, &m_code);

		if(err)
		{
			const auto* const errString = DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		m_dsp.regs().a.var = 0x0022334455667788;
		m_dsp.regs().b.var = 0x00bbccddeeff0011;

		m_dsp.regs().x.var = 0x00babeb00bf00d12;
		m_dsp.regs().y.var = 0x00bada55c0deba5e;
		func(0x123456);
	}

	Jit::~Jit()
	{
		delete m_asm;
	}

	void Jit::loadAGUtoXMM(int _xmm, int _agu)
	{
		// TODO: we can do better by reordering the DSP registers in memory so that we can load one 128 bit word at once
		m_asm->movd(xmm(_xmm), Jitmem::ptr(*m_asm, m_dsp.regs().m[_agu]));
		m_asm->pslldq(xmm(_xmm), Imm(4));

		m_asm->movd(xmm(15), Jitmem::ptr(*m_asm, m_dsp.regs().n[_agu]));
		m_asm->movss(xmm(_xmm), xmm(15));
		m_asm->pslldq(xmm(_xmm), Imm(4));

		m_asm->movd(xmm(15), Jitmem::ptr(*m_asm, m_dsp.regs().r[_agu]));
		m_asm->movss(xmm(_xmm), xmm(15));
	}

	void Jit::loadALUtoXMM(int _xmm, int _alu)
	{
		auto& alu = _alu ? m_dsp.regs().b : m_dsp.regs().a;

		m_asm->movq(xmm(_xmm), Jitmem::ptr(*m_asm, alu));
	}

	void Jit::loadXYtoXMM(int _xmm, int _xy)
	{
		auto& xy = _xy ? m_dsp.regs().x : m_dsp.regs().y;

		m_asm->movq(xmm(_xmm), Jitmem::ptr(*m_asm, xy));
	}

	void Jit::storeAGUfromXMM(int _agu, int _xmm)
	{
		const auto xm = xmm(_xmm);
		m_asm->movd(Jitmem::ptr(*m_asm, m_dsp.regs().r[_agu]), xm);
		m_asm->psrldq(xm, Imm(4));
		m_asm->movd(Jitmem::ptr(*m_asm, m_dsp.regs().n[_agu]), xm);
		m_asm->psrldq(xm, Imm(4));
		m_asm->movd(Jitmem::ptr(*m_asm, m_dsp.regs().m[_agu]), xm);
	}

	void Jit::storeALUfromXMM(int _alu, int _xmm)
	{
		auto& alu = _alu ? m_dsp.regs().b : m_dsp.regs().a;

		m_asm->movq(Jitmem::ptr(*m_asm, alu), xmm(_xmm));
	}

	void Jit::storeXYfromXMM(int _xy, int _xmm)
	{
		auto& xy = _xy ? m_dsp.regs().x : m_dsp.regs().y;

		m_asm->movq(Jitmem::ptr(*m_asm, xy), xmm(_xmm));
	}

	void Jit::loadDSPRegs()
	{
		for(auto i=0; i<8; ++i)
			loadAGUtoXMM(xmmR0 + i, i);

		loadALUtoXMM(xmmA, 0);
		loadALUtoXMM(xmmB, 1);

		loadXYtoXMM(xmmX, 0);
		loadXYtoXMM(xmmY, 1);
	}

	void Jit::storeDSPRegs()
	{
		for(auto i=0; i<8; ++i)
			storeAGUfromXMM(i, xmmR0 + i);

		storeALUfromXMM(0, xmmA);
		storeALUfromXMM(1, xmmB);

		storeXYfromXMM(0, xmmX);
		storeXYfromXMM(1, xmmY);
	}
}
