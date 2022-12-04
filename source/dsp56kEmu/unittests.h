#pragma once

#include "dsp.h"
#include "jitops.h"
#include "memory.h"
#include "peripherals.h"

#define verify(S)												\
{																\
	if(!(S)) 													\
	{															\
		assert(false && "Unit test failed: " #S);				\
		LOG("Unit Test failed: " << (#S));						\
		throw std::string("JIT Unit Test failed: " #S);			\
	}															\
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

		void aguModulo();
		void aguMultiWrapModulo();
		void aguBitreverse();

		void abs();
		void add();
		void addShortImmediate();
		void addLongImmediate();
		void addl();
		void addr();
		void and_();
		void andi();
		void asl_D();
		void asl_ii();
		void asl_S1S2D();
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
		void dec();

		void dmac();
		void eor();
		void extractu();
		void ifcc();
		void inc();
		void insert();
		void lra();
		void lsl();
		void lsr();
		void lua_ea();
		void lua_rn();
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
		void tfr();

		void move();
		void parallel();

		Peripherals56362 peripheralsX;
		Peripherals56367 peripheralsY;
		Memory mem;
		DSP dsp;
	};
}
