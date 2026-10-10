#include "interpreterunittests.h"

#include "agu.h"
#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	InterpreterUnitTests::InterpreterUnitTests()
	{
		testCCCC();
		testSubr();
		testCycleAccounting();
		
		runAllTests();

		// last: it runs the DSP through 400 interrupted loops, which the timer tests must not inherit
		testInterruptKeepsLazyCCR();
	}

	void InterpreterUnitTests::testInterruptKeepsLazyCCR()
	{
		// An interrupt taken between a cmp and the Bcc that tests it must not change where the branch goes.
		// In a JIT build, execInterrupt runs the vector through the JIT even while the DSP runs on the
		// interpreter; the JIT reads SR as it is, so a JSR in the vector used to stack an SR without the
		// condition codes the interpreter had not computed yet (ccrCache), and the RTI brought back stale
		// bits: the branch after the cmp went the wrong way.
		if constexpr(!g_useJIT)
			return;

		dsp.resetHW();

		// $100: move #$43,r1   $101: clr b x:(r1),a   $102: cmp #3,a   $103: ble $100   $104: (wrong way)
		const TWord loop[] = {0x314300, 0x56e11b, 0x014385, 0x05f7dd};
		for(TWord i = 0; i < 4; ++i)
			dsp.memWriteP(0x100 + i, loop[i]);
		dsp.memWriteP(0x104, 0x000000);
		dsp.memWriteP(0x76, 0x0bf080);	// ESAI_1 receive last slot vector: jsr $300
		dsp.memWriteP(0x77, 0x000300);
		dsp.memWriteP(0x300, 0x000000);	// nop
		dsp.memWriteP(0x301, 0x000004);	// rti
		dsp.memWrite(MemArea_X, 0x43, 0);			// 0 <= 3: the branch is always taken
		dsp.memWrite(MemArea_X, 0xFFFFFF, 0xaa0000);	// IPRC
		dsp.memWrite(MemArea_X, 0xFFFFFE, 0x000431);	// IPRP
		auto sr = dsp.regs().sr;
		sr.var &= ~0x300u;							// interrupt mask 0
		dsp.regs().sr = sr;
		dsp.setPC(0x100);

		// the interrupt is injected after each of the loop's instructions in turn; after the cmp is the case
		int wrong = 0;
		for(int iter = 0; iter < 400; ++iter)
		{
			for(int k = 0; k < 8 && dsp.getPC().toWord() != 0x100; ++k)
				dsp.execInterpreter();
			for(int k = 0; k < iter % 4; ++k)
				dsp.execInterpreter();
			dsp.injectInterrupt(0x76);
			for(int k = 0; k < 10; ++k)
			{
				dsp.execInterpreter();
				if(dsp.getPC().toWord() == 0x104)
				{
					++wrong;
					dsp.setPC(0x100);
					break;
				}
			}
		}
		verify(wrong == 0);

		// leave no interrupt set-up behind for the tests that follow
		dsp.memWrite(MemArea_X, 0xFFFFFF, 0);
		dsp.memWrite(MemArea_X, 0xFFFFFE, 0);
		dsp.resetHW();
	}

	void InterpreterUnitTests::testCycleAccounting()
	{
		if constexpr(g_useJIT)
		{
			// Normal JIT builds must not pay for the interpreter-only per-PC cache.
			verify(dsp.m_opcodeCycleCache.empty());
			return;
		}

		verify(dsp.m_opcodeCycleCache.size() == dsp.memory().sizeP());

		// A cached instruction cost is used on execution and invalidated by P writes.
		dsp.resetHW();
		execOpcode(assembler.assemble("nop").word[0], 0, false, 0x100);
		verify(dsp.getCycles() == 1);
		verify(dsp.m_opcodeCycleCache[0x100] == 1);

		const auto andi = assembler.assemble("andi #$33,mr");
		verify(andi.success());
		dsp.memWriteP(0x100, andi.word[0]);
		if(andi.wordCount > 1)
			dsp.memWriteP(0x101, andi.word[1]);
		verify(dsp.m_opcodeCycleCache[0x100] == 0);
		dsp.setPC(0x100);
		dsp.execInterpreter();
		verify(dsp.getCycles() == 4);
		verify(dsp.m_opcodeCycleCache[0x100] == 3);

		// REP executes its own instruction plus the repeated body inside one interpreter step.
		dsp.resetHW();
		TWord pc = 0x100;
		pc = emitToMemory("rep #$4", pc);
		emitToMemory("nop", pc);
		dsp.setPC(0x100);
		dsp.execInterpreter();
		verify(dsp.getCycles() == 9); // REP (5) + four NOPs (1 each)

		// DO likewise runs its loop body internally rather than returning through execOp per pass.
		dsp.resetHW();
		pc = 0x100;
		pc = emitToMemory("do #$5,>$104", pc);
		pc = emitToMemory("nop", pc);
		emitToMemory("nop", pc);
		dsp.setPC(0x100);
		dsp.execInterpreter();
		verify(dsp.getCycles() == 15); // DO (5) + five two-NOP iterations
	}

	void InterpreterUnitTests::execOpcode(uint32_t _op0, uint32_t _op1, const bool _reset, TWord _pc)
	{
		if(_reset)
			dsp.resetHW();
		dsp.clearOpcodeCache();
		dsp.mem.set(MemArea_P, _pc, _op0);
		dsp.mem.set(MemArea_P, _pc + 1, _op1);
		dsp.setPC(_pc);

		// Execute only the instruction, bypassing interrupt handling which
		// is designed for a running DSP, not single-step unit tests.
		dsp.pcCurrentInstruction = _pc;
		const auto op = dsp.fetchPC();
		dsp.execOp(op);
	}

	void InterpreterUnitTests::testSubr()
	{
		dsp.setALU(false, TReg56(static_cast<TReg56::MyType>(0x00600000000000)));
		dsp.setALU(true , TReg56(static_cast<TReg56::MyType>(0x00020000000000)));

		emit("subr b,a");
		verify(dsp.aluA().var == 0x002e0000000000);
		verify(!dsp.sr_test(CCR_C));
		verify(!dsp.sr_test(CCR_V));
	}

	void InterpreterUnitTests::testCCCC()
	{
		constexpr auto T=true;
		constexpr auto F=false;

		//                            <  <= =  >= >  != 
		testCCCC(0xff000000000000, 0, T, T, F, F, F, T);
		testCCCC(0x00ff0000000000, 0, F, F, F, T, T, T);
		testCCCC(0x00000000000000, 0, F, T, T, T ,F ,F);
	}

	void InterpreterUnitTests::testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq)
	{
		dsp.resetHW();
		dsp.setALU(false, TReg56(static_cast<TReg56::MyType>(_value)));
		dsp.alu_cmp(false, TReg56(_compareValue), false);
		char sr[16]{};
		dsp.sr_debug(sr);
		verify(_lt == (dsp.decode_cccc(CCCC_LessThan) != 0));
		verify(_le == (dsp.decode_cccc(CCCC_LessEqual) != 0));
		verify(_eq == (dsp.decode_cccc(CCCC_Equal) != 0));
		verify(_ge == (dsp.decode_cccc(CCCC_GreaterEqual) != 0));
		verify(_gt == (dsp.decode_cccc(CCCC_GreaterThan) != 0));
		verify(_neq == (dsp.decode_cccc(CCCC_NotEqual) != 0));	
	}

	void InterpreterUnitTests::runTest(const std::function<void()>& _build, const std::function<void()>& _verify)
	{
		_build();
		_verify();
	}

	void InterpreterUnitTests::emit(TWord _opA, TWord _opB, TWord _pc)
	{
		execOpcode(_opA, _opB, false, _pc);
	}

}
