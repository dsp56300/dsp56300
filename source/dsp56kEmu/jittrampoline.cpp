#include "jittrampoline.h"

#include "dsp.h"
#include "jitemitter.h"
#include "jitmem.h"
#include "jitprofilingsupport.h"
#include "jitregtypes.h"

namespace dsp56k
{
#ifdef HAVE_ARM64
	constexpr auto g_funcToCall = g_nonVolatileGPs[1];
	constexpr auto g_ptrDSP = g_nonVolatileGPs[2];
	constexpr auto g_ptrJit = g_nonVolatileGPs[3];
	constexpr auto g_counter = g_nonVolatileGPs[4];
	constexpr auto g_temp = g_nonVolatileGPs[5];
#else
	constexpr auto g_funcToCall = asmjit::x86::rbx;
	constexpr auto g_ptrDSP = asmjit::x86::rbp;
	constexpr auto g_ptrJit = asmjit::x86::r12;
	constexpr auto g_counter = asmjit::x86::r13;
	constexpr auto g_temp = asmjit::x86::r14;
#endif

	static_assert(!g_funcToCall.equals(regDspPtr));
	static_assert(!g_ptrDSP.equals(regDspPtr));
	static_assert(!g_ptrJit.equals(regDspPtr));
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
		m_asm.push(r64(g_ptrJit));
		m_asm.push(r64(g_counter));
		m_asm.push(r64(g_temp));

#ifdef HAVE_ARM64
		m_asm.push(asmjit::a64::regs::x30);
#endif

#ifdef _WIN32
		m_asm.sub(asmjit::x86::regs::rsp, asmjit::Imm(32));	// shadow space
#endif

		m_asm.mov(r64(g_ptrDSP), r64(g_funcArgGPs[0]));
		m_asm.mov(r32(g_counter), r32(g_funcArgGPs[1]));

		const auto label = m_asm.newNamedLabel("beginExec8times");
		m_asm.bind(label);

		const auto ptrJit           = Jitmem::makeRelativePtr(&m_dsp.getJit()          , &m_dsp, g_ptrDSP, 8);
		const auto ptrJitEntries    = Jitmem::makeRelativePtr(&m_dsp.getJitEntries()   , &m_dsp, g_ptrDSP, 8);
		const auto ptrInterruptFunc = Jitmem::makeRelativePtr(&m_dsp.getInterruptFunc(), &m_dsp, g_ptrDSP, 8);
		const auto ptrPC            = Jitmem::makeRelativePtr(&m_dsp.regs().pc.var     , &m_dsp, g_ptrDSP, 4);

		assert(ptrJit.offset());
		assert(ptrJitEntries.offset());
		assert(ptrInterruptFunc.offset());
		assert(ptrPC.offset());

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
			m_asm.ldr(g_funcToCall, Jitmem::makePtr(g_funcToCall, g_funcArgGPs[1], 3));

			m_asm.add(r64(g_funcArgGPs[0]), g_ptrDSP, asmjit::Imm(ptrJit.offset()));

			m_asm.blr(g_funcToCall);
#else
			m_asm.mov(g_temp, ptrInterruptFunc);
			m_asm.mov(g_funcArgGPs[0], g_ptrDSP);
			m_asm.call(g_temp);

			m_asm.mov(g_funcToCall, ptrJitEntries);
			m_asm.mov(r32(g_funcArgGPs[1]), ptrPC);
			m_asm.mov(g_funcToCall, Jitmem::makePtr(g_funcToCall, g_funcArgGPs[1], 3, 8));

			m_asm.lea(r64(g_funcArgGPs[0]), ptrJit);

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

		m_asm.pop(r64(g_temp));
		m_asm.pop(r64(g_counter));
		m_asm.pop(r64(g_ptrJit));
		m_asm.pop(r64(g_ptrDSP));
		m_asm.pop(r64(g_funcToCall));

		m_asm.ret();
		m_asm.finalize();
		m_runtime.add(&m_funcExecLoop, &codeHolder);

		if (auto* profiling = m_dsp.getJit().getProfilingSupport())
		{
			profiling->addFunction("trampolineExecLoop", reinterpret_cast<void*>(m_funcExecLoop), codeHolder);
			LOG("ADD TRAMPOLINE: trampolineExecLoop");
		}
		else
		{
			LOG("NO PROFILER AVAILABLE");
		}
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

//		m_asm.mov(regDspPtr, g_funcArgGPs[0]);

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

	void JitTrampoline::generateGetDspRegPtrFunc()
	{
		asmjit::CodeHolder codeHolder;
		codeHolder.init(m_runtime.environment());
		codeHolder.setLogger(&m_logger);
		codeHolder.setErrorHandler(&m_errorHandler);

		JitEmitter m_asm(&codeHolder);

		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateAssembler);

		{
			m_asm.mov(regReturnVal, regDspPtr);
			m_asm.ret();
		}
	
		m_asm.finalize();
		m_runtime.add(&m_funcGetDspPtr, &codeHolder);

		if (auto* profiling = m_dsp.getJit().getProfilingSupport())
			profiling->addFunction("trampolineGetDspRegPtr", reinterpret_cast<void*>(m_funcGetDspPtr), codeHolder);
	}
}
