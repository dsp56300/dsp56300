#pragma once

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"

#include "jittypes.h"

namespace dsp56k
{
	class DSP;

	class Jit
	{
	public:
		Jit(DSP& _dsp);
		~Jit();

	private:
		DSP& m_dsp;

		void loadAGUtoXMM(int _xmm, int _agu);
		void loadALUtoXMM(int _xmm, int _alu);
		void loadXYtoXMM(int _xmm, int _xy);

		void storeAGUfromXMM(int _agu, int _xmm);
		void storeALUfromXMM(int _alu, int _xmm);
		void storeXYfromXMM(int _xy, int _xmm);

		template<typename T>
		void getR(T _dst, int _agu)
		{
			m_asm->movd(_dst, asmjit::x86::xmm(xmmR0+_agu));
		}

		template<typename T>
		void getN(T _dst, int _agu)
		{
			const auto xm(asmjit::x86::xmm(xmmR0+_agu));

			m_asm->pshufd(xm, xm, asmjit::Imm(0xe1));		// swap lower two words to get N in word 0
			m_asm->movd(_dst, xm);
			m_asm->pshufd(xm, xm, asmjit::Imm(0xe1));		// swap back
		}

		template<typename T>
		void getM(T _dst, int _agu)
		{
			const auto xm(asmjit::x86::xmm(xmmR0+_agu));
			m_asm->pshufd(xm, xm, asmjit::Imm(0xc6));		// swap words 0 and 2 to ret M in word 0
			m_asm->movd(_dst, xm);
			m_asm->pshufd(xm, xm, asmjit::Imm(0xc6));		// swap back
		}

		void loadDSPRegs();
		void storeDSPRegs();

		asmjit::JitRuntime m_rt;
		asmjit::CodeHolder m_code;
		asmjit::x86::Assembler* m_asm = nullptr;
	};
}
