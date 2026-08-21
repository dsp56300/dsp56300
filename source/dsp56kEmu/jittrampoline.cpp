#include "jittrampoline.h"

#include "dsp.h"
#include "jitemitter.h"
#include "jitmem.h"
#include "jitprofilingsupport.h"
#include "jitregtypes.h"

namespace dsp56k
{
#ifdef HAVE_ARM64
	constexpr auto g_ptrDSP = JitReg64(22);
	constexpr auto g_counter = JitReg64(23);
	constexpr auto g_ptrJitEntries = JitReg64(24);
	constexpr auto g_ptrPC = JitReg64(25);
	constexpr auto g_ptrInterruptFunc = JitReg64(26);
	// hoisted constants for the inlined "is a peripheral due" test
	constexpr auto g_periphFunc = JitReg64(19);
	constexpr auto g_ptrTargetClock = JitReg64(21);
	constexpr auto g_ptrInstructions = JitReg64(27);
#else
	constexpr auto g_ptrDSP = asmjit::x86::r12;
	constexpr auto g_counter = asmjit::x86::r13;
	constexpr auto g_ptrJitEntries = asmjit::x86::r14;
	constexpr auto g_ptrPC = asmjit::x86::r15;
	constexpr auto g_ptrInterruptFunc = asmjit::x86::rbp;
#endif

	constexpr auto g_funcToCall = g_funcArgGPs[2];

	static_assert(!g_ptrInterruptFunc.equals(regDspPtr));
	static_assert(!g_ptrDSP.equals(regDspPtr));
	static_assert(!g_counter.equals(regDspPtr));
	static_assert(!g_ptrJitEntries.equals(regDspPtr));
	static_assert(!g_ptrPC.equals(regDspPtr));

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

#ifdef HAVE_ARM64
		// fill nonvolatile registers with pointers that we need
		m_asm.push(r64(g_ptrDSP));
		m_asm.push(r64(g_counter));
		m_asm.push(r64(regDspPtr));
		m_asm.push(r64(g_ptrJitEntries));
		m_asm.push(r64(g_ptrInterruptFunc));
		m_asm.push(r64(g_ptrPC));
		m_asm.push(r64(g_periphFunc));
		m_asm.push(r64(g_ptrTargetClock));
		m_asm.push(r64(g_ptrInstructions));
#else
		// regDspPtr survives the whole loop: no block uses it and C++ callees preserve it. Everything
		// else we need would be destroyed by a block now, so it lives in our own frame instead.
		m_asm.push(r64(regDspPtr));
		for (const auto& gp : g_trampolineSavedGPs)
			m_asm.push(r64(gp));
#endif

#ifdef HAVE_ARM64
		m_asm.push(asmjit::a64::regs::x30);
#endif

#ifdef HAVE_X86_64
#ifdef _WIN32
		static constexpr uint32_t g_shadow = 32;
#else
		static constexpr uint32_t g_shadow = 0;
#endif
		static constexpr uint32_t g_slotDsp = g_shadow;			// DSP*
		static constexpr uint32_t g_slotCounter = g_shadow + 8;	// loop counter
		// rsp is 8 mod 16 on entry; keep it 0 mod 16 at the call whatever the push count is
		static constexpr int g_pushCount = static_cast<int>(std::size(g_trampolineSavedGPs)) + 1;
		static constexpr int g_slotBytes = static_cast<int>(g_shadow) + 16;
		static constexpr uint32_t g_additionalStackSize = static_cast<uint32_t>(g_slotBytes + (((8 - 8*g_pushCount - g_slotBytes) % 16 + 16) % 16));
		static_assert(((8 - 8*g_pushCount - static_cast<int>(g_additionalStackSize)) % 16) == 0, "rsp must be 16 byte aligned at the call");
		m_asm.sub(asmjit::x86::regs::rsp, asmjit::Imm(g_additionalStackSize));
#endif

		const auto argDspPtr = r64(g_funcArgGPs[0]);
		const auto argCounter = r32(g_funcArgGPs[1]);

#ifdef HAVE_ARM64
		m_asm.mov(r64(g_ptrDSP), argDspPtr);
		m_asm.mov(r32(g_counter), argCounter);
#else
		m_asm.mov(asmjit::x86::ptr(asmjit::x86::regs::rsp, g_slotDsp, 8), argDspPtr);
		m_asm.mov(asmjit::x86::ptr(asmjit::x86::regs::rsp, g_slotCounter, 4), argCounter);
#endif

		m_asm.lea_(g_ptrJitEntries   , argDspPtr, &m_dsp.getJitEntries(), &m_dsp);
		m_asm.lea_(g_ptrInterruptFunc, argDspPtr, &m_dsp.getInterruptFunc(), &m_dsp);
		m_asm.lea_(g_ptrPC           , argDspPtr, &m_dsp.regs().pc.var, &m_dsp);

		// Constants for the inlined peripherals test. perif[0] is an IPeripherals* const and
		// m_execPeripheralsFunc never changes after construction, so both are fixed here.
		const auto* const periphFunc = reinterpret_cast<const void*>(m_dsp.getExecPeripheralsFunc());
		const auto* const targetClock = m_dsp.getPeriph(0)->getTargetClockPtr();

#ifdef HAVE_ARM64
		m_asm.mov(g_periphFunc, asmjit::Imm(periphFunc));
		m_asm.mov(g_ptrTargetClock, asmjit::Imm(targetClock));
		m_asm.mov(g_ptrInstructions, asmjit::Imm(&m_dsp.getInstructionCounter()));
#endif

		const auto ptrDspRegs = Jitmem::makeRelativePtr(&m_dsp.regs(), &m_dsp, argDspPtr, 8);
		assert(ptrDspRegs.offset());

#ifdef HAVE_ARM64
		m_asm.add(regDspPtr, g_ptrDSP, ptrDspRegs.offset());
#else
		m_asm.lea(regDspPtr, ptrDspRegs);
#endif

		const auto label = m_asm.newNamedLabel("beginExec8times");
		// align the loop head: asmjit does not do this for us, and an unaligned hot loop shows up
		// as frontend fetch stalls
		m_asm.align(asmjit::AlignMode::kCode, 64);
		m_asm.bind(label);

		for(uint32_t i=0; i<UnrollSize; ++i)
		{
			// 1) call interrupt func: (DSP*)
			// 2) call JIT func:       (DspRegs*, PC)

#ifdef HAVE_ARM64
			// The interrupt func is dspExecPeripherals ~99% of the time and that one does nothing but
			// "if(targetClock > instructions) return". Inlining that test here turns a guaranteed indirect
			// call per block entry into two loads and a not-taken branch. Must stay exactly equivalent to
			// DSP::execPeriph().
			const auto lCallInt = m_asm.newLabel();
			const auto lSkipInt = m_asm.newLabel();

			m_asm.ldr(g_funcToCall, Jitmem::makePtr(g_ptrInterruptFunc, 8));
			m_asm.cmp(g_funcToCall, g_periphFunc);
			m_asm.b(asmjit::arm::CondCode::kNE, lCallInt);
			m_asm.ldr(r64(g_funcArgGPs[0]), Jitmem::makePtr(g_ptrTargetClock, 8));
			m_asm.ldr(r64(g_funcArgGPs[1]), Jitmem::makePtr(g_ptrInstructions, 8));
			m_asm.cmp(r64(g_funcArgGPs[0]), r64(g_funcArgGPs[1]));
			m_asm.b(asmjit::arm::CondCode::kHI, lSkipInt);
			m_asm.bind(lCallInt);
			m_asm.mov(g_funcArgGPs[0], g_ptrDSP);
			m_asm.blr(g_funcToCall);
			m_asm.bind(lSkipInt);

			m_asm.ldr(g_funcToCall, Jitmem::makePtr(g_ptrJitEntries, 8));
			m_asm.ldr(r32(g_funcArgGPs[1]), Jitmem::makePtr(g_ptrPC, 4));
			m_asm.ldr(g_funcToCall, Jitmem::makePtr(g_funcToCall, g_funcArgGPs[1], 3, 8));
			m_asm.mov(r64(g_funcArgGPs[0]), regDspPtr);
			m_asm.blr(g_funcToCall);
#else
			const auto dsp = asmjit::x86::rax;	// volatile, reloaded after every call

			// see the ARM path above for why this test is inlined
			const auto lCallInt = m_asm.newLabel();
			const auto lSkipInt = m_asm.newLabel();
			const auto scratchA = asmjit::x86::r10;	// volatile in both x64 ABIs, and we are before the call
			const auto scratchB = asmjit::x86::r11;

			m_asm.mov(dsp, asmjit::x86::ptr(asmjit::x86::regs::rsp, g_slotDsp, 8));
			m_asm.mov(g_funcToCall, Jitmem::makeRelativePtr(&m_dsp.getInterruptFunc(), &m_dsp, dsp, 8));
			m_asm.mov(scratchA, asmjit::Imm(periphFunc));
			m_asm.cmp(g_funcToCall, scratchA);
			m_asm.jne(lCallInt);
			m_asm.mov(scratchA, asmjit::Imm(targetClock));
			m_asm.mov(scratchB, asmjit::x86::ptr(scratchA, 0, 8));
			m_asm.cmp(scratchB, Jitmem::makeRelativePtr(&m_dsp.getInstructionCounter(), &m_dsp, dsp, 8));
			m_asm.ja(lSkipInt);
			m_asm.bind(lCallInt);
			m_asm.mov(g_funcArgGPs[0], dsp);
			m_asm.call(g_funcToCall);
			m_asm.bind(lSkipInt);

			m_asm.mov(dsp, asmjit::x86::ptr(asmjit::x86::regs::rsp, g_slotDsp, 8));
			m_asm.mov(g_funcToCall, Jitmem::makeRelativePtr(&m_dsp.getJitEntries(), &m_dsp, dsp, 8));
			m_asm.mov(r32(g_funcArgGPs[1]), Jitmem::makeRelativePtr(&m_dsp.regs().pc.var, &m_dsp, dsp, 4));
			m_asm.mov(g_funcToCall, Jitmem::makePtr(g_funcToCall, g_funcArgGPs[1], 3, 8));
			m_asm.mov(r64(g_funcArgGPs[0]), regDspPtr);
			m_asm.call(g_funcToCall);
#endif
		}

#ifdef HAVE_ARM64
		m_asm.subs(r32(g_counter), r32(g_counter), asmjit::Imm(1));
#else
		m_asm.dec(asmjit::x86::ptr(asmjit::x86::regs::rsp, g_slotCounter, 4));
#endif
		m_asm.jnz(label);

#ifdef HAVE_X86_64
		m_asm.add(asmjit::x86::regs::rsp, asmjit::Imm(g_additionalStackSize));
#endif

#ifdef HAVE_ARM64
		m_asm.pop(asmjit::a64::regs::x30);
#endif
#ifdef HAVE_ARM64
		m_asm.pop(r64(g_ptrInstructions));
		m_asm.pop(r64(g_ptrTargetClock));
		m_asm.pop(r64(g_periphFunc));
		m_asm.pop(r64(g_ptrPC));
		m_asm.pop(r64(g_ptrInterruptFunc));
		m_asm.pop(r64(g_ptrJitEntries));
		m_asm.pop(r64(regDspPtr));
		m_asm.pop(r64(g_counter));
		m_asm.pop(r64(g_ptrDSP));
#else
		for (size_t i=std::size(g_trampolineSavedGPs); i-- > 0;)
			m_asm.pop(r64(g_trampolineSavedGPs[i]));
		m_asm.pop(r64(regDspPtr));
#endif

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
#ifndef HAVE_ARM64
		// blocks no longer preserve these, so this entry has to do it for its C++ caller
		for (const auto& gp : g_trampolineSavedGPs)
			m_asm.push(r64(gp));
#endif

		m_asm.mov(regDspPtr, g_funcArgGPs[0]);

#ifdef HAVE_X86_64
#ifdef _WIN32
		static constexpr int g_oneShadow = 32;
#else
		static constexpr int g_oneShadow = 0;
#endif
		// rsp is 8 mod 16 on entry; the push count above varies, so derive the rest of the frame
		static constexpr int g_onePushes = static_cast<int>(std::size(g_trampolineSavedGPs)) + 1;
		static constexpr uint32_t g_oneStackSize = static_cast<uint32_t>(g_oneShadow + (((8 - 8*g_onePushes - g_oneShadow) % 16 + 16) % 16));
		static_assert(((8 - 8*g_onePushes - static_cast<int>(g_oneStackSize)) % 16) == 0, "rsp must be 16 byte aligned at the call");
		m_asm.sub(asmjit::x86::regs::rsp, asmjit::Imm(g_oneStackSize));
		m_asm.call(g_funcArgGPs[2]);
		m_asm.add(asmjit::x86::regs::rsp, asmjit::Imm(g_oneStackSize));
#else
		m_asm.push(asmjit::a64::regs::x30);

		m_asm.blr(g_funcArgGPs[2]);

		m_asm.pop(asmjit::a64::regs::x30);
#endif

#ifndef HAVE_ARM64
		for (size_t i=std::size(g_trampolineSavedGPs); i-- > 0;)
			m_asm.pop(r64(g_trampolineSavedGPs[i]));
#endif
		m_asm.pop(regDspPtr);

		m_asm.ret();
		m_asm.finalize();
		m_runtime.add(&m_funcExecOne, &codeHolder);

		if (auto* profiling = m_dsp.getJit().getProfilingSupport())
			profiling->addFunction("trampolineExecOne", reinterpret_cast<void*>(m_funcExecOne), codeHolder);
	}
}
