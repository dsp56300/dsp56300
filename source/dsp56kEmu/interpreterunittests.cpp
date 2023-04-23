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
		
		runAllTests();
	}

	void InterpreterUnitTests::execOpcode(uint32_t _op0, uint32_t _op1, const bool _reset, TWord _pc)
	{
		if(_reset)
			dsp.resetHW();
		dsp.clearOpcodeCache();
		dsp.mem.set(MemArea_P, _pc, _op0);
		dsp.mem.set(MemArea_P, _pc + 1, _op1);
		dsp.setPC(_pc);
		dsp.exec();
	}

	void InterpreterUnitTests::testSubr()
	{
		dsp.reg.a.var = 0x00600000000000;
		dsp.reg.b.var = 0x00020000000000;

		// subr b,a
		emit(0x200006);
		verify(dsp.reg.a.var == 0x002e0000000000);
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
		dsp.reg.a.var = _value;
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
