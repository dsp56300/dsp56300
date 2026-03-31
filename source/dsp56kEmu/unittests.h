#pragma once

#include "assembler.h"
#include "dsp.h"
#include "jitops.h"
#include "memory.h"
#include "peripherals.h"

#define verify(S)																																				\
{																																								\
	if(!(S)) 																																					\
	{																																							\
		assert(false && "Unit test failed: " #S);																												\
		LOG("Unit Test in func " << __func__ << " at line " << __LINE__ << " failed: " << (#S));																\
		throw std::string("Unit test in func ")  + std::string(__func__) + std::string(" line ") + std::to_string(__LINE__) + std::string(" failed: "  #S);		\
	}																																							\
}

namespace  dsp56k
{
	class UnitTests
	{
	protected:
		UnitTests();

		virtual void runTest(const std::function<void()>& _build, const std::function<void()>& _verify) = 0;
		virtual void emit(TWord _opA, TWord _opB = 0, TWord _pc = 0) = 0;
		void emit(const char* _text, TWord _pc = 0);

		// Write assembled instruction to P memory (for multi-instruction execution)
		TWord emitToMemory(const char* _text, TWord _pc);
		TWord emitToMemory(TWord _opA, TWord _opB, TWord _pc);

		// Run DSP from current PC until _targetPC is reached or _maxCycles exceeded.
		// Uses execStep() which is overridden by JIT/interpreter test runners.
		uint32_t execUntil(TWord _targetPC, uint32_t _maxCycles = 10000);
		virtual void execStep() = 0;

		Assembler assembler;

		void runAllTests();

		void conditionCodes();
		void aguModulo();
		void aguMultiWrapModulo();
		void aguBitreverse();

		void x0x1Combinations();

		void abs();
		void add();
		void addShortImmediate();
		void addLongImmediate();
		void addl();
		void addr();
		void and_();
		void andi();
		void asl();
		void asl_D();
		void asl_ii();
		void asl_S1S2D();
		void asr();
		void asr_D();
		void asr_ii();
		void asr_S1S2D();

		void bchg_aa();
		void bclr_ea();
		void bclr_aa();
		void bclr_qqpp();
		void bclr_D();
		void bset_aa();
		void btst_aa();

		void clb();
		void clr();
		void cmp();
		void cmpm();
		void dec();
		void div();

		void dmac();
		void eor();
		void extractu();
		void extractu_co();
		void ifcc();
		void inc();
		void insert();
		void jscc();
		void lra();
		void lsl();
		void lsr();
		void lua_ea();
		void lua_rn();
		void mac();
		void mac_S();
		void max();
		void maxm();
		void mpy();
		void mpyr();
		void mpy_SD();
		void neg();
		void normf();
		void not_();
		void or_();
		void ori();
		void rnd();
		void rol();
		void sub();
		void subl();
		void tfr();
		void tcc();

		void move();
		void movel();
		void parallel();

		// ALU extended
		void and_xxxx();
		void or_xxxx();
		void sub_xxxx();
		void cmp_xxxx();
		void subr();
		void mpyi();
		void mpy_su();
		void tst();
		void nop();

		// branches
		void bra();
		void bcc();
		void bsr();
		void bscc();
		void brclr_brset();
		void bsclr_bsset();

		// jumps
		void jmp();
		void jcc();
		void jsr();
		void jclr_jset();
		void jsclr_jsset();

		// bit manipulation extended
		void bchg();
		void bset();
		void btst();

		// newly implemented
		void eor_xx();
		void ror_();

		// bit-test jump/branch — peripheral addressing modes
		void jclr_jset_ppqq();
		void jsclr_jsset_ppqq();
		void brclr_brset_ppqq();

		// multi-instruction tests
		void multiInstructionTests();
		void rep_multi();
		void do_multi();
		void jsr_rts();

		Peripherals56362 peripheralsX;
		Peripherals56367 peripheralsY;
		Memory mem;
		DSP dsp;
	};
}
