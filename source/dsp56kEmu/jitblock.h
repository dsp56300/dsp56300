#pragma once

#include "jitdspregs.h"
#include "jitmem.h"
#include "jitregtracker.h"
#include "jittypes.h"

namespace asmjit
{
	namespace x86
	{
		class Assembler;
	}
}

namespace dsp56k
{
	class DSP;

	class JitBlock
	{
	public:
		JitBlock(asmjit::x86::Assembler& _a, DSP& _dsp)
		: m_asm(_a)
		, m_dsp(_dsp)
		, m_xmmPool({regXMMTempA, regXMMTempB, regXMMTempC})
		, m_gpPool({regGPTempA, regGPTempB, regGPTempC, regGPTempD})
		, m_dspRegs(*this)
		, m_mem(*this)
		{
		}

		asmjit::x86::Assembler& asm_() { return m_asm; }
		DSP& dsp() { return m_dsp; }
		JitRegpool<asmjit::x86::Gpq>& gpPool() { return m_gpPool; }
		JitRegpool<asmjit::x86::Xmm>& xmmPool() { return m_xmmPool; }
		JitDspRegs& regs() { return m_dspRegs; }
		Jitmem& mem() { return m_mem; }

		operator JitRegpool<asmjit::x86::Gpq>& ()		{ return m_gpPool; }
		operator JitRegpool<asmjit::x86::Xmm>& ()		{ return m_xmmPool;	}

	private:
		asmjit::x86::Assembler& m_asm;
		DSP& m_dsp;
		JitRegpool<asmjit::x86::Xmm> m_xmmPool;
		JitRegpool<asmjit::x86::Gpq> m_gpPool;
		JitDspRegs m_dspRegs;
		Jitmem m_mem;
	};
}
