#include "interpreterunittests.h"

#include "agu.h"
#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	InterpreterUnitTests::InterpreterUnitTests()
	{
		testMoveImmediateToRegister();
		testMoveMemoryToRegister();
		testMoveXYOverlap();
		testCCCC();
		testJSGE();
		testMultiply();
		testAdd();
		testAddr();
		testSub();
		testSubl();
		testSubr();
		testCMP();
		testASL();
		testASR();
		testMAC();
		testLongMemoryMoves();
		testDIV();
		testROL();
		testNOT();
		testEXTRACTU();
		testEXTRACTU_CO();
		testMPY();
		
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

	void InterpreterUnitTests::testASL()
	{
		dsp.reg.a.var = 0xaaabcdef123456;

		// asl #1,a,a
		execOpcode(0x0c1d02);
		verify(dsp.reg.a.var == 0x55579bde2468ac);

		// asr #1,a,a
		execOpcode(0x0c1c02);
		verify(dsp.reg.a.var == 0x2aabcdef123456);

		// asl b
		dsp.reg.b.var = 0x000599f2204000;
		execOpcode(0x20003a);
		verify(dsp.reg.b.var == 0x000b33e4408000);

		// asl #28,a,a
		dsp.reg.a.var = 0xf4;
		dsp.setSR(0x0800d0);
		execOpcode(0x0c1d50);
		verify(dsp.reg.a.var == 0x00f40000000000);
		verify(dsp.getSR().var == 0x0800f0);
	}

	void InterpreterUnitTests::testASR()
	{
		dsp.reg.a.var = 0x000599f2204000;

		// asr a
		execOpcode(0x200022);
		verify(dsp.reg.a.var == 0x0002ccf9102000);
	}

	void InterpreterUnitTests::testMultiply()
	{
		// mpy x0,y0,a
		testMultiply(0xeeeeee, 0xbbbbbb, 0x00091a2bd4c3b4, 0x2000d0);
		testMultiply(0xffffff, 0x7fffff, 0xffffffff000002, 0x2000d0);
		testMultiply(0xffffff, 0xffffff, 0x00000000000002, 0x2000d0);		
	}

	void InterpreterUnitTests::testMultiply(int x0, int y0, int64_t expectedResult, TWord opcode)
	{
		dsp.reg.x.var = x0;
		dsp.reg.y.var = y0;

		// a = x0 * y0
		execOpcode(opcode);		verify(dsp.reg.a == expectedResult);
	}

	void InterpreterUnitTests::testMoveImmediateToRegister()
	{
		// move #$ff,a
		execOpcode(0x2eff00);		verify(dsp.reg.a == 0x00ffff0000000000);

		// move #$0f,a
		execOpcode(0x2e0f00);		verify(dsp.reg.a == 0x00000f0000000000);

		// move #$ff,x0
		execOpcode(0x24ff00);		verify(dsp.x0() == 0xff0000);		verify(dsp.reg.x == 0xff0000);

		// move #$0f,r2
		execOpcode(0x32ff00);		verify(dsp.reg.r[2] == 0x0000ff);

		// move #$12,a2
		execOpcode(0x2a1200);

		// move #$345678,a1
		execOpcode(0x54f400, 0x345678);

		// move #$abcdef,a0
		execOpcode(0x50f400, 0xabcdef);		verify(dsp.reg.a.var == 0x0012345678abcdef);

		// move a,b
		execOpcode(0x21cf00);				verify(dsp.reg.b.var == 0x00007fffff000000);
	}

	void InterpreterUnitTests::testMoveMemoryToRegister()
	{
		dsp.reg.r[5].var = 10;
		dsp.memory().set(MemArea_Y, 9, 0x123456);
		dsp.reg.b.var = 0;

		// move y:-(r5),b)
		execOpcode(0x5ffd00);
		verify(dsp.reg.b.var == 0x00123456000000);
		verify(dsp.reg.r[5].var == 9);
	}

	void InterpreterUnitTests::testMoveXYOverlap()
	{
		dsp.memory().set(MemArea_X, 10, 0x123456);
		dsp.memory().set(MemArea_Y, 5, 0x543210);
		dsp.reg.a.var = 0x0000babeb00bab;

		dsp.reg.r[2].var = 10;
		dsp.reg.r[6].var = 5;

		// move x:(r2)+,a a,y:(r6)+
		execOpcode(0xbada00);
		verify(dsp.reg.r[2] == 11);
		verify(dsp.reg.r[6] == 6);

		verify(dsp.reg.a == 0x00123456000000);
		verify(dsp.memory().get(MemArea_X, 10) == 0x123456);
		verify(dsp.memory().get(MemArea_Y, 5 ) == 0xbabe);
	}

	void InterpreterUnitTests::testAdd()
	{
		testAdd(0, 0, 0);
		testAdd(0x00000000123456, 0x000abc, 0x00000abc123456);
		testAdd(0x00000000123456, 0xabcdef, 0xffabcdef123456);	// TODO: test CCR

		dsp.reg.a.var = 0x0001e000000000;
		dsp.reg.b.var = 0xfffe2000000000;

		// add b,a
		execOpcode(0x200010);
		verify(dsp.reg.a.var == 0);
		verify(dsp.sr_test(CCR_C));
		verify(!dsp.sr_test(CCR_V));
	}

	void InterpreterUnitTests::testAddr()
	{
		dsp.reg.a.var = 0x004edffe000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800d0);							// (S L) U
		execOpcode(0x200002);	// addr b,a
		verify(dsp.reg.a.var == 0x0ffb16e12000000);
		verify(dsp.getSR().var == 0x0800c8);			// (S L) N

		dsp.reg.a.var = 0xffb16e12000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800c8);							// (S L) N
		execOpcode(0x20000a);	// addr a,b
		verify(dsp.reg.b.var == 0xff766d1b800000);
		verify(dsp.getSR().var == 0x0800e9);			// (S L) E N C
	}

	void InterpreterUnitTests::testSub()
	{
		dsp.reg.a.var = 0x00000000000001;
		dsp.reg.b.var = 0x00000000000002;

		// sub b,a
		execOpcode(0x200014);
		verify(dsp.reg.a.var == 0xffffffffffffff);
		verify(dsp.sr_test(CCR_C));
		verify(!dsp.sr_test(CCR_V));

		dsp.reg.a.var = 0x80000000000000;
		dsp.reg.b.var = 0x00000000000001;

		// sub b,a
		execOpcode(0x200014);
		verify(dsp.reg.a.var == 0x7fffffffffffff);
		verify(!dsp.sr_test(CCR_C));
		verify(!dsp.sr_test(CCR_V));
	}

	void InterpreterUnitTests::testSubl()
	{
		dsp.reg.a.var = 0x00400000000000;
		dsp.reg.b.var = 0x00200000000000;

		// subl b,a
		execOpcode(0x200016);
		verify(dsp.reg.a.var == 0x00600000000000);
		verify(!dsp.sr_test(CCR_C));
		verify(!dsp.sr_test(CCR_V));
	}

	void InterpreterUnitTests::testSubr()
	{
		dsp.reg.a.var = 0x00600000000000;
		dsp.reg.b.var = 0x00020000000000;

		// subr b,a
		execOpcode(0x200006);
		verify(dsp.reg.a.var == 0x002e0000000000);
		verify(!dsp.sr_test(CCR_C));
		verify(!dsp.sr_test(CCR_V));
	}

	void InterpreterUnitTests::testAdd(int64_t a, int y0, int64_t expectedResult)
	{
		dsp.reg.a.var = a;
		dsp.reg.y.var = y0;

		// add y0,a
		execOpcode(0x200050);

		verify(dsp.reg.a.var == expectedResult);
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

	void InterpreterUnitTests::testJSGE()
	{
		// SR is the result of a being 0x0055000000000000 and then: tst a
		dsp.setSR(0x0800c0);

		dsp.reg.r[2].var = 0x50;

		// jsge (r2)
		execOpcode(0x0be2a1);
		verify(dsp.getPC() == 0x50);
	}

	void InterpreterUnitTests::testCMP()
	{
		dsp.reg.b.var = 0;
		dsp.b1(TReg24(0x123456));

		dsp.reg.x.var = 0;
		dsp.x0(TReg24(0x123456));

		// cmp x0,b
		execOpcode(0x20004d);

		verify(dsp.sr_test(CCR_Z));
		verify(!dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_E | CCR_V | CCR_C)));

		dsp.x0(0xf00000);
		dsp.reg.a.var = 0xfff40000000000;
		dsp.setSR(0x0800d8);

		// cmp x0,a
		execOpcode(0x200045);
		verify(dsp.getSR().var == 0x0800d0);

		// cmp #>$aa,a
		dsp.setSR(0x080099);
		dsp.reg.a.var = 0xfffffc6c000000;
		execOpcode(0x0140c5, 0x0000aa);
		verify(dsp.getSR().var == 0x080098);

		{
			dsp.regs().b.var = 0;
			dsp.b1(TReg24(0x123456));

			dsp.regs().x.var = 0;
			dsp.x0(TReg24(0x123456));

			execOpcode(0x20004d);		// cmp x0,b

			verify(dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_E));
			verify(!dsp.sr_test(CCR_V));
			verify(!dsp.sr_test(CCR_C));
		}
		{
			dsp.x0(0xf00000);
			dsp.regs().a.var = 0xfff40000000000;
			dsp.setSR(0x0800d8);

			execOpcode(0x200045);	// cmp x0,a

			verify(dsp.getSR().var == 0x0800d0);

			dsp.setSR(0x080099);
			dsp.regs().a.var = 0xfffffc6c000000;
			execOpcode(0x0140c5, 0x0000aa);	// cmp #>$aa,a

			verify(dsp.getSR().var == 0x080098);
		}
	}

	void InterpreterUnitTests::testMAC()
	{
		dsp.reg.x.var =   0xda7efa5a7efa;
		dsp.reg.y.var =   0x000000800000;
		dsp.reg.a.var = 0x005a7efa000000;
		dsp.reg.b.var = 0x005a7efa000000;

		// mac x1,y0,a
		execOpcode(0x2000e2);
		verify(dsp.reg.a == 0x00800000000000);

		// mac y0,x0,b 
		execOpcode(0x2000da);
		verify(dsp.reg.b == 0x00000000000000);

		dsp.y0(0x7fffff);
		dsp.x0(0x6bb14a);
		dsp.reg.b.var = 0x00553300000000;
		dsp.setSR(0x0880d0);

		// mac y0,x0,b 
		execOpcode(0x2000da);
		verify(dsp.reg.b == 0x00c0e449289d6c);
		verify(dsp.getSR().var == 0x0880f0);

		// mac y1,y0,b x:(r5)-,y0
		dsp.y1(0xf3aab8);
		dsp.y0(0x000080);
		dsp.setSR(0x0800d8);
		dsp.reg.b.var = 0x0000000c000000;
		dsp.reg.r[5].var = 10;
		dsp.memory().set(MemArea_X, 10, 0x123456);

		execOpcode(0x46d5bb);

		verify(dsp.reg.b == 0);
		verify(dsp.reg.r[5].var == 9);
		verify(dsp.y0() == 0x123456);
		verify(dsp.getSR().var == 0x0800d4);
	}

	void InterpreterUnitTests::testLongMemoryMoves()
	{
		mem.set(MemArea_X, 100, 0x123456);
		mem.set(MemArea_Y, 100, 0x345678);

		dsp.reg.r[0].var = 100;

		// move l:(r0),ab
		execOpcode(0x4ae000);
		verify(dsp.reg.a.var == 0x00123456000000);
		verify(dsp.reg.b.var == 0x00345678000000);

		dsp.reg.b.var = 0xaabadbadbadbad;
		dsp.memory().set(MemArea_X, 10, 0x123456);
		dsp.memory().set(MemArea_Y, 10, 0x543210);
		dsp.reg.r[0].var = 10;

		// move l:(r0),b
		execOpcode(0x49e000);
		verify(dsp.reg.b == 0x00123456543210);
	}

	void InterpreterUnitTests::testDIV()
	{
		{
			dsp.setSR(dsp.getSR().var & 0xfe);

			constexpr uint64_t expectedValues[24] =
			{
				0xffef590e000000,
				0xffef790e000000,
				0xffefb90e000000,
				0xfff0390e000000,
				0xfff1390e000000,
				0xfff3390e000000,
				0xfff7390e000000,
				0xffff390e000000,
				0x000f390e000000,
				0x000dab2a000001,
				0x000a8f62000003,
				0x000457d2000007,
				0xfff7e8b200000f,
				0x0000985600001e,
				0xfff069ba00003d,
				0xfff19a6600007a,
				0xfff3fbbe0000f4,
				0xfff8be6e0001e8,
				0x000243ce0003d0,
				0xfff3c0aa0007a1,
				0xfff84846000f42,
				0x0001577e001e84,
				0xfff1e80a003d09,
				0xfff49706007a12
			};

			dsp.reg.a.var = 0x00001000000000;
			dsp.reg.y.var = 0x04444410c6f2;

			for (size_t i = 0; i < 24; ++i)
			{
				// div y0,a
				execOpcode(0x018050);
				verify(dsp.reg.a.var == expectedValues[i]);
			}
		}

		{
			dsp.y0(0x218dec);
			dsp.reg.a.var = 0x00008000000000;
			dsp.setSR(0x0800d4);

			constexpr uint64_t expectedValues[24] =
			{
				0xffdf7214000000,
				0xffe07214000000,
				0xffe27214000000,
				0xffe67214000000,
				0xffee7214000000,
				0xfffe7214000000,
				0x001e7214000000,
				0x001b563c000001,
				0x00151e8c000003,
				0x0008af2c000007,
				0xffefd06c00000f,
				0x00012ec400001e,
				0xffe0cf9c00003d,
				0xffe32d2400007a,
				0xffe7e8340000f4,
				0xfff15e540001e8,
				0x00044a940003d0,
				0xffe7073c0007a1,
				0xffef9c64000f42,
				0x0000c6b4001e84,
				0xffdfff7c003d09,
				0xffe18ce4007a12,
				0xffe4a7b400f424,
				0xffeadd5401e848
			};

			for (size_t i = 0; i < 24; ++i)
			{
				// div y0,a
				execOpcode(0x018050);
				verify(dsp.reg.a.var == expectedValues[i]);
			}
		}
	}

	void InterpreterUnitTests::testROL()
	{
		dsp.sr_set(CCR_C);
		dsp.reg.a.var = 0x12abcdef123456;				// 00010010 10101011 11001101 11101111 00010010 00110100 01010110

		// rol a
		execOpcode(0x200037);

		verify(dsp.reg.a.var == 0x12579BDF123456);		// 00010010 01010111 10011011 11011111 00010010 00110100 01010110
		verify(dsp.sr_test(CCR_C) == 1);

		dsp.sr_set(CCR_C);
		dsp.reg.a.var = 0x12123456abcdef;				// 00010010 00010010 00110100 01010110 10101011 11001101 11101111

		// rol a
		execOpcode(0x200037);

		verify(dsp.reg.a.var == 0x122468ADABCDEF);		// 00010010 00100100 01101000 10101101 10101011 11001101 11101111
		verify(dsp.sr_test(CCR_C) == 0);
	}

	void InterpreterUnitTests::testNOT()
	{
		dsp.reg.a.var = 0x12555555123456;

		// not a
		execOpcode(0x200017);

		verify(dsp.reg.a.var == 0x12aaaaaa123456);

		dsp.reg.a.var = 0xffd8b38b000000;
		dsp.setSR(0x0800e8);

		// not a
		execOpcode(0x200017);
		verify(dsp.reg.a.var == 0xff274c74000000);
		verify(dsp.getSR().var == 0x0800e0);
	}

	void InterpreterUnitTests::testEXTRACTU()
	{
		dsp.reg.x.var = 0x4008000000;  // x1 = 0x4008  (width=4, offset=8)
		dsp.reg.a.var = 0xff00;

		// extractu x1,a,b  (width = 0x8, offset = 0x28)
		execOpcode(0x0c1a8d);

		verify(dsp.reg.b.var == 0xf);

		dsp.reg.a.var = 0;
		dsp.reg.b.var = 0xfff47555000000;
		dsp.setSR(0x0800d9);

		// extractu $8028,b,a
		execOpcode(0x0c1890, 0x008028);

		verify(dsp.reg.a.var == 0xf4);
		verify(dsp.getSR().var == 0x0800d0);
	}

	void InterpreterUnitTests::testEXTRACTU_CO()
	{
		dsp.reg.b.var = 0x0444ffff000000;

		// extractu #$C028,b,a  (width = 0xC, offset = 0x28)
		execOpcode(0x0c1890, 0x00C028);

		verify(dsp.reg.a.var == 0x444);
	}

	void InterpreterUnitTests::testMPY()
	{
		dsp.reg.a.var = 0x00400000000000;
		dsp.reg.b.var = 0x0003a400000000;
		dsp.reg.x.var = 0x00000506c000;
		dsp.reg.y.var = 0x000400000400;
		dsp.setSR(0x0800c9);

		// mpy y0,x0,a
		execOpcode(0x2000d0);
		verify(dsp.reg.a.var == 0x00000036000000);
		verify(dsp.getSR().var == 0x0800d1);

		dsp.x0(0xef4e);
		dsp.y0(0x600000);
		dsp.setSR(0x0880d0);
		dsp.reg.omr.var = 0x004380;

		execOpcode(0x2000d1);		// mpyr y0,x0,a

		verify(dsp.reg.a.var == 0x0000b37a000000);
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
