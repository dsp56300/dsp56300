#include "jit.h"

#include "jitmem.h"

#include "dsp.h"

#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"

namespace dsp56k
{
	Jit::Jit(DSP& _dsp) : m_dsp(_dsp)
	{
		m_code.init(m_rt.environment());

		asmjit::x86::Assembler a(&m_code);

		asmjit::x86::Mem memA = Jithelper::ptr(m_dsp.regs().a);
		asmjit::x86::Mem memB = Jithelper::ptr(m_dsp.regs().b);

		a.mov(asmjit::x86::rax, memB);
		a.mov(memA, asmjit::x86::rax);
		a.ret();

		typedef void (*Func)(TWord op);

		Func func;

		const auto err = m_rt.add(&func, &m_code);

		m_dsp.regs().a.var = 0x1122334455667788;
		m_dsp.regs().b.var = 0xaabbccddeeff0011;
		func(0x123456);
	}

	Jit::~Jit()
	{
	}
}
