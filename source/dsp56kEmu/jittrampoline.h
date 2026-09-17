#pragma once

#include <cstdint>
#include <vector>

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
		typedef void (*TExecOneFunc)(JitDspPtr*, TWord, TJitFunc) noexcept;	// DspRegs, PC, function to be called

		static constexpr uint32_t UnrollShift = 3;
		static constexpr uint32_t UnrollSize = 1<<UnrollShift;

		JitTrampoline(DSP& _dsp);

		void generateCode()
		{
			generateExecLoopFunc();
			generateExecOneFunc();
		}

		void exec(DSP* _dsp, uint32_t _count) noexcept
		{
			assert(((_count >> UnrollShift) << UnrollShift) == _count);
			++m_running;
			m_funcExecLoop(_dsp, _count >> UnrollShift);
			stopped();
		}

		void execOne(JitDspPtr* _jit, const TWord _pc, const TJitFunc _func) noexcept
		{
			++m_running;
			m_funcExecOne(_jit, _pc, _func);
			stopped();
		}

		/*	Generated code calls peripherals and external bus devices, and those can destroy blocks by writing P memory. The
			block that made the call can be one of them, or it jumps to one of them when it ends, so the code of a destroyed
			block is released once no generated code runs anymore.
		*/
		void releaseCode(TJitFunc _func);

	private:
		void stopped() noexcept
		{
			if(!--m_running && !m_retiredCode.empty())
				releaseRetiredCode();
		}

		void releaseRetiredCode() noexcept;

		void initCodeHolder(asmjit::CodeHolder& _codeHolder);
		void generateExecLoopFunc();
		void generateExecOneFunc();

		DSP& m_dsp;
		asmjit::JitRuntime m_runtime;
		AsmJitLogger m_logger;
		AsmJitErrorHandler m_errorHandler;
		TExecLoopFunc m_funcExecLoop = nullptr;
		TExecOneFunc m_funcExecOne = nullptr;

		uint32_t m_running = 0;					// exec and execOne nest, generated code runs while this is not zero
		std::vector<TJitFunc> m_retiredCode;
	};
}
