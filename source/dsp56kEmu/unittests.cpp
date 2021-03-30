#include "pch.h"

#include "unittests.h"

#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
#define T true
#define F false

	UnitTests::UnitTests() : mem(&peripherals, &peripherals), dsp(mem)
	{
		testMoveImmediateToRegister();
		testCCCC();
		testMultiply();
		testAdd();
	}

	void UnitTests::testMultiply()
	{
		// mpy x0,y0,a
		testMultiply(0xeeeeee, 0xbbbbbb, 0x00091a2bd4c3b4, 0x2000d0);
		testMultiply(0xffffff, 0x7fffff, 0xffffffff000002, 0x2000d0);
		testMultiply(0xffffff, 0xffffff, 0x00000000000002, 0x2000d0);		
	}

	void UnitTests::testMultiply(int x0, int y0, int64_t expectedResult, TWord opcode)
	{
		dsp.reg.x.var = x0;
		dsp.reg.y.var = y0;

		// a = x0 * y0
		execOpcode(opcode);		assert(dsp.reg.a == expectedResult);
	}

	void UnitTests::testMoveImmediateToRegister()
	{
		// move #$ff,a
		execOpcode(0x2eff00);		assert(dsp.reg.a == 0x00ffff0000000000);

		// move #$0f,a
		execOpcode(0x2e0f00);		assert(dsp.reg.a == 0x00000f0000000000);

		// move #$ff,x0
		execOpcode(0x24ff00);		assert(dsp.x0() == 0xff0000);		assert(dsp.reg.x == 0xff0000);

		// move #$0f,r2
		execOpcode(0x32ff00);		assert(dsp.reg.r[2] == 0x0000ff);

		// move #$12,a2
		execOpcode(0x2a1200);

		// move #$345678,a1
		execOpcode(0x54f400, 0x345678);

		// move #$abcdef,a2
		execOpcode(0x50f400, 0xabcdef);		assert(dsp.reg.a.var == 0x0012345678abcdef);

		// move a,b
		execOpcode(0x21cf00);				assert(dsp.reg.b.var == 0x00007fffff000000);
	}

	void UnitTests::testAdd()
	{
		testAdd(0, 0, 0);
		testAdd(0x00000000123456, 0x000abc, 0x00000abc123456);
		testAdd(0x00000000123456, 0xabcdef, 0xffabcdef123456);	// TODO: test CCR
	}

	void UnitTests::testAdd(int64_t a, int y0, int64_t expectedResult)
	{
		dsp.reg.a.var = a;
		dsp.reg.y.var = y0;

		// add y0,a
		execOpcode(0x200050);

		assert(dsp.reg.a.var == expectedResult);
	}

	void UnitTests::testCCCC()
	{
		//                            <  <= =  >= >  != 
		testCCCC(0xff000000000000, 0, T, T, F, F, F, T);
		testCCCC(0x00ff0000000000, 0, F, F, F, T, T, T);
		testCCCC(0x00000000000000, 0, F, T, T, T ,F ,F);
	}

	void UnitTests::testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq)
	{
		dsp.resetHW();
		dsp.reg.a.var = _value;
		dsp.alu_cmp(false, TReg56(_compareValue), false);
		char sr[16]{};
		dsp.sr_debug(sr);
		assert(_lt == dsp.decode_cccc(CCCC_LessThan));
		assert(_le == dsp.decode_cccc(CCCC_LessEqual));
		assert(_eq == dsp.decode_cccc(CCCC_Equal));
		assert(_ge == dsp.decode_cccc(CCCC_GreaterEqual));
		assert(_gt == dsp.decode_cccc(CCCC_GreaterThan));
		assert(_neq == dsp.decode_cccc(CCCC_NotEqual));	
	}

	void UnitTests::execOpcode(uint32_t _op0, uint32_t _op1, const bool _reset)
	{
		if(_reset)
			dsp.resetHW();
		dsp.mem.set(MemArea_P, 0, _op0);
		dsp.mem.set(MemArea_P, 1, _op1);
		dsp.setPC(0);
		dsp.exec();
	}
}
