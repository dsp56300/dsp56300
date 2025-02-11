#pragma once

#include <cstdint>

#include "jitasmjithelpers.h"
#include "jittypes.h"

#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	class DSP;

	class JitTrampoline
	{
	public:
		typedef void (*TExecLoopFunc)(DSP*, uint32_t) noexcept;				// DSP, iteration count
		typedef void (*TExecOneFunc)(JitDspPtr*, TWord, TJitFunc) noexcept;	// Jit*, PC, JIT function
		typedef JitDspPtr* (*TGetDspPtrFunc)() noexcept;					// returns pointer to Jit*

		static constexpr uint32_t UnrollShift = 3;
		static constexpr uint32_t UnrollSize = 1<<UnrollShift;

		JitTrampoline(DSP& _dsp);

		void generateCode()
		{
			generateExecLoopFunc();
			generateExecOneFunc();
			generateGetDspRegPtrFunc();
		}

		void exec(DSP* _dsp, uint32_t _count) const noexcept
		{
			assert(((_count >> UnrollShift) << UnrollShift) == _count);
			m_funcExecLoop(_dsp, _count >> UnrollShift);
		}

		void execOne(JitDspPtr* _jit, const TWord _pc, const TJitFunc _func) const noexcept
		{
			m_funcExecOne(_jit, _pc, _func);
		}

	private:
		void generateExecLoopFunc();
		void generateExecOneFunc();
		void generateGetDspRegPtrFunc();

		DSP& m_dsp;
		asmjit::JitRuntime m_runtime;
		AsmJitLogger m_logger;
		AsmJitErrorHandler m_errorHandler;
		TExecLoopFunc m_funcExecLoop = nullptr;
		TExecOneFunc m_funcExecOne = nullptr;
		TGetDspPtrFunc m_funcGetDspPtr = nullptr;
	};
}
