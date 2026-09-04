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

		// Scratch peripheral address for the tests that exercise pp addressing. It has to be inside
		// the pp range ($ffffc0-$ffffff) AND not be a register any peripheral models, so that a
		// written value reads back unchanged. The tests used $ffffd0 until the DSP56362 DAX took
		// that address and broke them, so it is named here rather than spelled out per test.
		static constexpr TWord g_testPeriphAddr = 0xfffffe;

		// the same address as assembler operand text, so it is only written down in one place
		static std::string testPeriphAddrStr();

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
		void cmpu();
		void mpyri();
		void merge();
		void enddo();
		void bitmodOnSR();
		void unimplementedOpcodeLength();
		void dec();
		void div();

		void dmac();
		void dmacMultiPrecision();
		void eor();
		void extract();
		void extractu();
		void extractu_co();
		void ifcc();
		void inc();
		void insert();
		void saBitfield();
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
		void tfr_signextend();
		void tcc();

		void move();
		void sixteenBitArithmeticMoves();
		void mergeSixteenBit();
		void movel();
		void parallel();

		// ALU extended
		void and_xxxx();
		void or_xxxx();
		void sub_xxxx();
		void cmp_xxxx();
		void subr();
		void mpyi();
		void maci_xxxx();
		void mpy_su();
		void macsu_unsigned();
		void mpyMacSignedUnsigned();
		void macr_rounded();
		void rnd_scalingModes();
		void limit_transfer_test();
		void max_ccr();
		void max_parallel();
		void ymem_parallel_write();
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
		void norm();
		void ror_();

		// bit-test jump/branch — peripheral addressing modes
		void jclr_jset_ppqq();
		void jsclr_jsset_ppqq();
		void brclr_brset_ppqq();

		// multi-instruction tests
		void multiInstructionTests();
		void rep_multi();
		void cmpu_multi();
		void brkcc_multi();
		void bitmodOnSR_deferredCCR();
		void rep_div_powerOfTwo();
		void do_multi();
		void do_forever();
		void verifyLoopRetired(uint32_t _expectedR0) const;
		void enableBranchAtLoopEnd();
		void do_twoWordCallAtLoopEnd();
		void enableDynamicFastInterrupts(bool _enable);
		void callAtVectorAddress();
		void callAfterRepAtVectorAddress();
		void conditionalCallAtVectorAddress();
		void callInsideLoopAtVectorAddress();
		void do_callAtLoopEnd();
		void do_callNotAtLoopEnd();
		void jsr_rts();

		Peripherals56362 peripheralsX;
		Peripherals56367 peripheralsY;
		Memory mem;
		DSP dsp;
	};
}
