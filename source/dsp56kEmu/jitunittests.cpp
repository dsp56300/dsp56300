#include "jitunittests.h"

#include "jitdspregs.h"
#include "jitops.h"

namespace dsp56k
{
	JitUnittests::JitUnittests()
	: mem(m_defaultMemoryValidator, 0x100)
	, dsp(mem, &peripherals, &peripherals)
	{
		m_code.init(m_rt.environment());

		m_asm.reset(new asmjit::x86::Assembler(&m_code));

		{
			JitDspRegs regs(*m_asm, dsp);

			JitOps ops(dsp.opcodes(), regs, *m_asm);

			dsp.regs().a.var = 0x00ff112233445566;
			dsp.regs().b.var = 0x0000aabbccddeeff;

			ops.op_Abs(0);
			ops.op_Abs(1);
		}

		m_asm->ret();

		typedef void (*Func)();
		Func func;
		const auto err = m_rt.add(&func, &m_code);
		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		func();
		m_code.reset();

		assert(dsp.regs().a == 0x00EEDDCCBBAA9A);
		assert(dsp.regs().b == 0x0000aabbccddeeff);
	}

	JitUnittests::~JitUnittests()
	{
		m_asm.reset();
	}
}
