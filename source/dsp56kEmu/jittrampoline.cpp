#include "jittrampoline.h"

#include "dsp.h"
#include "jitemitter.h"
#include "jitmem.h"
#include "jitprofilingsupport.h"
#include "jitregtypes.h"

namespace dsp56k
{
#ifdef HAVE_ARM64
	constexpr auto g_funcToCall = JitReg64(21);
	constexpr auto g_ptrDSP = JitReg64(22);
	constexpr auto g_counter = JitReg64(23);
	constexpr auto g_temp = JitReg64(24);
#else
	constexpr auto g_funcToCall = asmjit::x86::rsi;
	constexpr auto g_ptrDSP = asmjit::x86::rbp;
	constexpr auto g_counter = asmjit::x86::r13;
	constexpr auto g_temp = asmjit::x86::r14;
#endif

	static_assert(!g_funcToCall.equals(regDspPtr));
	static_assert(!g_ptrDSP.equals(regDspPtr));
	static_assert(!g_counter.equals(regDspPtr));
	static_assert(!g_temp.equals(regDspPtr));

	JitTrampoline::JitTrampoline(DSP& _dsp) : m_dsp(_dsp)
	{
	}

	void JitTrampoline::generateExecLoopFunc()
	{
		asmjit::CodeHolder codeHolder;
		codeHolder.init(m_runtime.environment());
		codeHolder.setLogger(&m_logger);
		codeHolder.setErrorHandler(&m_errorHandler);

		JitEmitter m_asm(&codeHolder);

		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateAssembler);

		// fill nonvolatile registers with pointers that we need
		m_asm.push(r64(g_funcToCall));
		m_asm.push(r64(g_ptrDSP));
		m_asm.push(r64(g_counter));
		m_asm.push(r64(g_temp));
		m_asm.push(r64(regDspPtr));

#ifdef HAVE_ARM64
		m_asm.push(asmjit::a64::regs::x30);
#endif

#ifdef _WIN32
		m_asm.sub(asmjit::x86::regs::rsp, asmjit::Imm(32));	// shadow space
#endif

		m_asm.mov(r64(g_ptrDSP), r64(g_funcArgGPs[0]));
		m_asm.mov(r32(g_counter), r32(g_funcArgGPs[1]));

		const auto ptrDspRegs       = Jitmem::makeRelativePtr(&m_dsp.regs()            , &m_dsp, g_ptrDSP, 8);
		const auto ptrJitEntries    = Jitmem::makeRelativePtr(&m_dsp.getJitEntries()   , &m_dsp, g_ptrDSP, 8);
		const auto ptrInterruptFunc = Jitmem::makeRelativePtr(&m_dsp.getInterruptFunc(), &m_dsp, g_ptrDSP, 8);
		const auto ptrPC            = Jitmem::makeRelativePtr(&m_dsp.regs().pc.var     , &m_dsp, g_ptrDSP, 4);

		assert(ptrDspRegs.offset());
		assert(ptrJitEntries.offset());
		assert(ptrInterruptFunc.offset());
		assert(ptrPC.offset());

#ifdef HAVE_ARM64
		m_asm.add(regDspPtr, g_ptrDSP, ptrDspRegs.offset());
#else
		m_asm.lea(regDspPtr, ptrDspRegs);
#endif
		const auto label = m_asm.newNamedLabel("beginExec8times");
		m_asm.bind(label);

		for(uint32_t i=0; i<UnrollSize; ++i)
		{
			// 1) call interrupt func first, arg0 = pointer to DSP
			// 2) call JIT func, arg0 = pointer to Jit, arg1 = PC

#ifdef HAVE_ARM64
			m_asm.mov(g_temp, ptrInterruptFunc);
			m_asm.mov(g_funcArgGPs[0], g_ptrDSP);

			m_asm.blr(g_temp);

			m_asm.mov(g_funcToCall, ptrJitEntries);
			m_asm.mov(r32(g_funcArgGPs[1]), ptrPC);
			m_asm.mov(r64(g_funcArgGPs[0]), regDspPtr);
			m_asm.ldr(g_funcToCall, Jitmem::makePtr(g_funcToCall, g_funcArgGPs[1], 3));

			m_asm.blr(g_funcToCall);
#else
			m_asm.mov(g_temp, ptrInterruptFunc);
			m_asm.mov(g_funcArgGPs[0], g_ptrDSP);
			m_asm.call(g_temp);

			m_asm.mov(g_funcToCall, ptrJitEntries);
			m_asm.mov(r32(g_funcArgGPs[1]), ptrPC);
			m_asm.mov(r64(g_funcArgGPs[0]), regDspPtr);
			m_asm.mov(g_funcToCall, Jitmem::makePtr(g_funcToCall, g_funcArgGPs[1], 3, 8));

			m_asm.call(g_funcToCall);
#endif
		}

#ifdef HAVE_ARM64
		m_asm.subs(r32(g_counter), r32(g_counter), asmjit::Imm(1));
#else
		m_asm.dec(r32(g_counter));
#endif
		m_asm.jnz(label);

#ifdef _WIN32
		m_asm.add(asmjit::x86::regs::rsp, asmjit::Imm(32));	// shadow space
#endif

#ifdef HAVE_ARM64
		m_asm.pop(asmjit::a64::regs::x30);
#endif

		m_asm.pop(r64(regDspPtr));
		m_asm.pop(r64(g_temp));
		m_asm.pop(r64(g_counter));
		m_asm.pop(r64(g_ptrDSP));
		m_asm.pop(r64(g_funcToCall));

		m_asm.ret();
		m_asm.finalize();
		m_runtime.add(&m_funcExecLoop, &codeHolder);

		if (auto* profiling = m_dsp.getJit().getProfilingSupport())
			profiling->addFunction("trampolineExecLoop", reinterpret_cast<void*>(m_funcExecLoop), codeHolder);
	}

	void JitTrampoline::generateExecOneFunc()
	{
		asmjit::CodeHolder codeHolder;
		codeHolder.init(m_runtime.environment());
		codeHolder.setLogger(&m_logger);
		codeHolder.setErrorHandler(&m_errorHandler);

		JitEmitter m_asm(&codeHolder);

		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateAssembler);

		m_asm.push(regDspPtr);

		m_asm.mov(regDspPtr, g_funcArgGPs[0]);

#ifdef _WIN32
		m_asm.sub(asmjit::x86::regs::rsp, asmjit::Imm(32));	// shadow space
		m_asm.call(g_funcArgGPs[2]);
		m_asm.add(asmjit::x86::regs::rsp, asmjit::Imm(32));	// shadow space
#else
		m_asm.push(asmjit::a64::regs::x30);
		m_asm.blr(g_funcArgGPs[2]);
		m_asm.pop(asmjit::a64::regs::x30);
#endif

		m_asm.pop(regDspPtr);

		m_asm.ret();
		m_asm.finalize();
		m_runtime.add(&m_funcExecOne, &codeHolder);

		if (auto* profiling = m_dsp.getJit().getProfilingSupport())
			profiling->addFunction("trampolineExecOne", reinterpret_cast<void*>(m_funcExecOne), codeHolder);
	}
}
