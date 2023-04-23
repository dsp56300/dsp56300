#pragma once

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

		Peripherals56362 peripheralsX;
		Peripherals56367 peripheralsY;
		Memory mem;
		DSP dsp;
	};
}
