#include "unittests.h"

#include "disasm.h"
#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
	static DefaultMemoryValidator g_defaultMemoryMap;
	
	UnitTests::UnitTests() : mem(g_defaultMemoryMap, 0x100), dsp(mem, &peripherals, &peripherals)
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

//		testDisassembler();		// will take a few minutes in debug, so commented out for now
	}

	void UnitTests::execOpcode(uint32_t _op0, uint32_t _op1, const bool _reset)
	{
		if(_reset)
			dsp.resetHW();
		dsp.clearOpcodeCache();
		dsp.mem.set(MemArea_P, 0, _op0);
		dsp.mem.set(MemArea_P, 1, _op1);
		dsp.setPC(0);
		dsp.exec();
	}

	void UnitTests::testASL()
	{
		dsp.reg.a.var = 0xaaabcdef123456;

		// asl #1,a,a
		execOpcode(0x0c1d02);
		assert(dsp.reg.a.var == 0x55579bde2468ac);

		// asr #1,a,a
		execOpcode(0x0c1c02);
		assert(dsp.reg.a.var == 0x2aabcdef123456);

		// asl b
		dsp.reg.b.var = 0x000599f2204000;
		execOpcode(0x20003a);
		assert(dsp.reg.b.var == 0x000b33e4408000);

		// asl #28,a,a
		dsp.reg.a.var = 0xf4;
		dsp.setSR(0x0800d0);
		execOpcode(0x0c1d50);
		assert(dsp.reg.a.var == 0x00f40000000000);
		assert(dsp.getSR().var == 0x0800f0);
	}

	void UnitTests::testASR()
	{
		dsp.reg.a.var = 0x000599f2204000;

		// asr a
		execOpcode(0x200022);
		assert(dsp.reg.a.var == 0x0002ccf9102000);		
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

		// move #$abcdef,a0
		execOpcode(0x50f400, 0xabcdef);		assert(dsp.reg.a.var == 0x0012345678abcdef);

		// move a,b
		execOpcode(0x21cf00);				assert(dsp.reg.b.var == 0x00007fffff000000);
	}

	void UnitTests::testMoveMemoryToRegister()
	{
		dsp.reg.r[5].var = 10;
		dsp.memory().set(MemArea_Y, 9, 0x123456);
		dsp.reg.b.var = 0;

		// move y:-(r5),b)
		execOpcode(0x5ffd00);
		assert(dsp.reg.b.var == 0x00123456000000);
		assert(dsp.reg.r[5].var == 9);
	}

	void UnitTests::testMoveXYOverlap()
	{
		dsp.memory().set(MemArea_X, 10, 0x123456);
		dsp.memory().set(MemArea_Y, 5, 0x543210);
		dsp.reg.a.var = 0x0000babeb00bab;

		dsp.reg.r[2].var = 10;
		dsp.reg.r[6].var = 5;

		// move x:(r2)+,a a,y:(r6)+
		execOpcode(0xbada00);
		assert(dsp.reg.r[2] == 11);
		assert(dsp.reg.r[6] == 6);

		assert(dsp.reg.a == 0x00123456000000);
		assert(dsp.memory().get(MemArea_X, 10) == 0x123456);
		assert(dsp.memory().get(MemArea_Y, 5 ) == 0xbabe);
	}

	void UnitTests::testAdd()
	{
		testAdd(0, 0, 0);
		testAdd(0x00000000123456, 0x000abc, 0x00000abc123456);
		testAdd(0x00000000123456, 0xabcdef, 0xffabcdef123456);	// TODO: test CCR

		dsp.reg.a.var = 0x0001e000000000;
		dsp.reg.b.var = 0xfffe2000000000;

		// add b,a
		execOpcode(0x200010);
		assert(dsp.reg.a.var == 0);
		assert(dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_V));
	}

	void UnitTests::testAddr()
	{
		dsp.reg.a.var = 0x004edffe000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800d0);							// (S L) U
		execOpcode(0x200002);	// addr b,a
		assert(dsp.reg.a.var == 0x0ffb16e12000000);
		assert(dsp.getSR().var == 0x0800c8);			// (S L) N

		dsp.reg.a.var = 0xffb16e12000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800c8);							// (S L) N
		execOpcode(0x20000a);	// addr a,b
		assert(dsp.reg.b.var == 0xff766d1b800000);
		assert(dsp.getSR().var == 0x0800e9);			// (S L) E N C
	}

	void UnitTests::testSub()
	{
		dsp.reg.a.var = 0x00000000000001;
		dsp.reg.b.var = 0x00000000000002;

		// sub b,a
		execOpcode(0x200014);
		assert(dsp.reg.a.var == 0xffffffffffffff);
		assert(dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_V));

		dsp.reg.a.var = 0x80000000000000;
		dsp.reg.b.var = 0x00000000000001;

		// sub b,a
		execOpcode(0x200014);
		assert(dsp.reg.a.var == 0x7fffffffffffff);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_V));
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
		constexpr auto T=true;
		constexpr auto F=false;

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
		assert(_lt == (dsp.decode_cccc(CCCC_LessThan) != 0));
		assert(_le == (dsp.decode_cccc(CCCC_LessEqual) != 0));
		assert(_eq == (dsp.decode_cccc(CCCC_Equal) != 0));
		assert(_ge == (dsp.decode_cccc(CCCC_GreaterEqual) != 0));
		assert(_gt == (dsp.decode_cccc(CCCC_GreaterThan) != 0));
		assert(_neq == (dsp.decode_cccc(CCCC_NotEqual) != 0));	
	}

	void UnitTests::testJSGE()
	{
		// SR is the result of a being 0x0055000000000000 and then: tst a
		dsp.setSR(0x0800c0);

		dsp.reg.r[2].var = 0x50;

		// jsge (r2)
		execOpcode(0x0be2a1);
		assert(dsp.getPC() == 0x50);
	}

	void UnitTests::testCMP()
	{
		dsp.reg.b.var = 0;
		dsp.b1(TReg24(0x123456));

		dsp.reg.x.var = 0;
		dsp.x0(TReg24(0x123456));

		// cmp x0,b
		execOpcode(0x20004d);

		assert(dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(static_cast<CCRMask>(SR_N | SR_E | SR_V | SR_C)));

		dsp.x0(0xf00000);
		dsp.reg.a.var = 0xfff40000000000;
		dsp.setSR(0x0800d8);

		// cmp x0,a
		execOpcode(0x200045);
		assert(dsp.getSR().var == 0x0800d0);

		// cmp #>$aa,a
		dsp.setSR(0x080099);
		dsp.reg.a.var = 0xfffffc6c000000;
		execOpcode(0x0140c5, 0x0000aa);
		assert(dsp.getSR().var == 0x080098);
	}

	void UnitTests::testMAC()
	{
		dsp.reg.x.var =   0xda7efa5a7efa;
		dsp.reg.y.var =   0x000000800000;
		dsp.reg.a.var = 0x005a7efa000000;
		dsp.reg.b.var = 0x005a7efa000000;

		// mac x1,y0,a
		execOpcode(0x2000e2);
		assert(dsp.reg.a == 0x00800000000000);

		// mac y0,x0,b 
		execOpcode(0x2000da);
		assert(dsp.reg.b == 0x00000000000000);

		dsp.y0(0x7fffff);
		dsp.x0(0x6bb14a);
		dsp.reg.b.var = 0x00553300000000;
		dsp.setSR(0x0880d0);

		// mac y0,x0,b 
		execOpcode(0x2000da);
		assert(dsp.reg.b == 0x00c0e449289d6c);
		assert(dsp.getSR().var == 0x0880f0);

		// mac y1,y0,b x:(r5)-,y0
		dsp.y1(0xf3aab8);
		dsp.y0(0x000080);
		dsp.setSR(0x0800d8);
		dsp.reg.b.var = 0x0000000c000000;
		dsp.reg.r[5].var = 10;
		dsp.memory().set(MemArea_X, 10, 0x123456);

		execOpcode(0x46d5bb);

		assert(dsp.reg.b == 0);
		assert(dsp.reg.r[5].var == 9);
		assert(dsp.y0() == 0x123456);
		assert(dsp.getSR().var == 0x0800d4);
	}

	void UnitTests::testLongMemoryMoves()
	{
		mem.set(MemArea_X, 100, 0x123456);
		mem.set(MemArea_Y, 100, 0x345678);

		dsp.reg.r[0].var = 100;

		// move l:(r0),ab
		execOpcode(0x4ae000);
		assert(dsp.reg.a.var == 0x00123456000000);
		assert(dsp.reg.b.var == 0x00345678000000);

		dsp.reg.b.var = 0xaabadbadbadbad;
		dsp.memory().set(MemArea_X, 10, 0x123456);
		dsp.memory().set(MemArea_Y, 10, 0x543210);
		dsp.reg.r[0].var = 10;

		// move l:(r0),b
		execOpcode(0x49e000);
		assert(dsp.reg.b == 0x00123456543210);
	}

	void UnitTests::testDIV()
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
		dsp.reg.y.var =   0x04444410c6f2;

		for(size_t i=0; i<24; ++i)
		{
			// div y0,a
			execOpcode(0x018050);
			assert(dsp.reg.a.var == expectedValues[i]);
		}

		dsp.y0(0x218dec);
		dsp.reg.a.var = 0x00008000000000;
		dsp.setSR(0x0800d4);

		// div y0,a
		execOpcode(0x018050);
		assert(dsp.reg.a.var == 0xffdf7214000000);
		assert(dsp.getSR().var == 0x0800d4);		
	}

	void UnitTests::testROL()
	{
		dsp.sr_set(SR_C);
		dsp.reg.a.var = 0x12abcdef123456;				// 00010010 10101011 11001101 11101111 00010010 00110100 01010110

		// rol a
		execOpcode(0x200037);

		assert(dsp.reg.a.var == 0x12579BDF123456);		// 00010010 01010111 10011011 11011111 00010010 00110100 01010110
		assert(dsp.sr_test(SR_C) == 1);

		dsp.sr_set(SR_C);
		dsp.reg.a.var = 0x12123456abcdef;				// 00010010 00010010 00110100 01010110 10101011 11001101 11101111

		// rol a
		execOpcode(0x200037);

		assert(dsp.reg.a.var == 0x122468ADABCDEF);		// 00010010 00100100 01101000 10101101 10101011 11001101 11101111
		assert(dsp.sr_test(SR_C) == 0);
	}

	void UnitTests::testNOT()
	{
		dsp.reg.a.var = 0x12555555123456;

		// not a
		execOpcode(0x200017);

		assert(dsp.reg.a.var == 0x12aaaaaa123456);

		dsp.reg.a.var = 0xffd8b38b000000;
		dsp.setSR(0x0800e8);

		// not a
		execOpcode(0x200017);
		assert(dsp.reg.a.var == 0xff274c74000000);
		assert(dsp.getSR().var == 0x0800e0);
	}

	void UnitTests::testEXTRACTU()
	{
		dsp.reg.x.var = 0x4008000000;  // x1 = 0x4008  (width=4, offset=8)
		dsp.reg.a.var = 0xff00;

		// extractu x1,a,b  (width = 0x8, offset = 0x28)
		execOpcode(0x0c1a8d);

		assert(dsp.reg.b.var == 0xf);

		dsp.reg.a.var = 0;
		dsp.reg.b.var = 0xfff47555000000;
		dsp.setSR(0x0800d9);

		// extractu $8028,b,a
		execOpcode(0x0c1890, 0x008028);

		assert(dsp.reg.a.var == 0xf4);
		assert(dsp.getSR().var == 0x0800d0);
	}

	void UnitTests::testEXTRACTU_CO()
	{
		dsp.reg.b.var = 0x0444ffff000000;

		// extractu #$C028,b,a  (width = 0xC, offset = 0x28)
		execOpcode(0x0c1890, 0x00C028);

		assert(dsp.reg.a.var == 0x444);
	}

	void UnitTests::testMPY()
	{
		dsp.reg.a.var = 0x00400000000000;
		dsp.reg.b.var = 0x0003a400000000;
		dsp.reg.x.var = 0x00000506c000;
		dsp.reg.y.var = 0x000400000400;
		dsp.setSR(0x0800c9);

		// mpy y0,x0,a
		execOpcode(0x2000d0);
		assert(dsp.reg.a.var == 0x00000036000000);
		assert(dsp.getSR().var == 0x0800d1);
	}

	void UnitTests::testDisassembler()
	{
#ifdef USE_MOTOROLA_UNASM
		LOG("Executing Disassembler Unit Tests");

		Opcodes opcodes;
		Disassembler disasm(opcodes);

		constexpr TWord opB = 0x234567;

		for(TWord i=0x000000; i<=0xffffff; ++i)
		{
			const TWord op = i;

			if((i&0x03ffff) == 0)
				LOG("Testing opcodes $" << HEX(i) << "+");
			
			char assemblyMotorola[128];
			std::string assembly;

			const auto opcodeCountMotorola = disassembleMotorola(assemblyMotorola, op, opB, 0, 0);

			if(opcodeCountMotorola > 0)
			{
				const int opcodeCount = disasm.disassemble(assembly, op, opB, 0, 0, 0);

				if(!opcodeCount)
					continue;

				std::string asmMot(assemblyMotorola);

				if(asmMot.back() == ' ')	// debugcc has extra space
					asmMot.pop_back();

				if(assembly != asmMot)
				{
					switch (op)
					{
					case 0x000006: if(assembly == "trap" && asmMot == "swi")                       continue; break;
					case 0x202015: if(assembly == "maxm a,b ifcc"   && asmMot == "max b,a ifcc")   continue; break;
					case 0x202115: if(assembly == "maxm a,b ifge"   && asmMot == "max b,a ifge")   continue; break;
					case 0x202215: if(assembly == "maxm a,b ifne"   && asmMot == "max b,a ifne")   continue; break;
					case 0x202315: if(assembly == "maxm a,b ifpl"   && asmMot == "max b,a ifpl")   continue; break;
					case 0x202415: if(assembly == "maxm a,b ifnn"   && asmMot == "max b,a ifnn")   continue; break;
					case 0x202515: if(assembly == "maxm a,b ifec"   && asmMot == "max b,a ifec")   continue; break;
					case 0x202615: if(assembly == "maxm a,b iflc"   && asmMot == "max b,a iflc")   continue; break;
					case 0x202715: if(assembly == "maxm a,b ifgt"   && asmMot == "max b,a ifgt")   continue; break;
					case 0x202815: if(assembly == "maxm a,b ifcs"   && asmMot == "max b,a ifcs")   continue; break;
					case 0x202915: if(assembly == "maxm a,b iflt"   && asmMot == "max b,a iflt")   continue; break;
					case 0x202a15: if(assembly == "maxm a,b ifeq"   && asmMot == "max b,a ifeq")   continue; break;
					case 0x202b15: if(assembly == "maxm a,b ifmi"   && asmMot == "max b,a ifmi")   continue; break;
					case 0x202c15: if(assembly == "maxm a,b ifnr"   && asmMot == "max b,a ifnr")   continue; break;
					case 0x202d15: if(assembly == "maxm a,b ifes"   && asmMot == "max b,a ifes")   continue; break;
					case 0x202e15: if(assembly == "maxm a,b ifls"   && asmMot == "max b,a ifls")   continue; break;
					case 0x202f15: if(assembly == "maxm a,b ifle"   && asmMot == "max b,a ifle")   continue; break;
					case 0x203015: if(assembly == "maxm a,b ifcc.u" && asmMot == "max b,a ifcc.u") continue; break;
					case 0x203115: if(assembly == "maxm a,b ifge.u" && asmMot == "max b,a ifge.u") continue; break;
					case 0x203215: if(assembly == "maxm a,b ifne.u" && asmMot == "max b,a ifne.u") continue; break;
					case 0x203315: if(assembly == "maxm a,b ifpl.u" && asmMot == "max b,a ifpl.u") continue; break;
					case 0x203415: if(assembly == "maxm a,b ifnn.u" && asmMot == "max b,a ifnn.u") continue; break;
					case 0x203515: if(assembly == "maxm a,b ifec.u" && asmMot == "max b,a ifec.u") continue; break;
					case 0x203615: if(assembly == "maxm a,b iflc.u" && asmMot == "max b,a iflc.u") continue; break;
					case 0x203715: if(assembly == "maxm a,b ifgt.u" && asmMot == "max b,a ifgt.u") continue; break;
					case 0x203815: if(assembly == "maxm a,b ifcs.u" && asmMot == "max b,a ifcs.u") continue; break;
					case 0x203915: if(assembly == "maxm a,b iflt.u" && asmMot == "max b,a iflt.u") continue; break;
					case 0x203a15: if(assembly == "maxm a,b ifeq.u" && asmMot == "max b,a ifeq.u") continue; break;
					case 0x203b15: if(assembly == "maxm a,b ifmi.u" && asmMot == "max b,a ifmi.u") continue; break;
					case 0x203c15: if(assembly == "maxm a,b ifnr.u" && asmMot == "max b,a ifnr.u") continue; break;
					case 0x203d15: if(assembly == "maxm a,b ifes.u" && asmMot == "max b,a ifes.u") continue; break;
					case 0x203e15: if(assembly == "maxm a,b ifls.u" && asmMot == "max b,a ifls.u") continue; break;
					case 0x203f15: if(assembly == "maxm a,b ifle.u" && asmMot == "max b,a ifle.u") continue; break;
					}

					LOG("Diff for opcode " << HEX(op) << ": " << assembly << " != " << assemblyMotorola);
					assert(false);
					disasm.disassemble(assembly, op, opB, 0, 0, 0);	// retry to help debugging
				}

				if(opcodeCount != opcodeCountMotorola)
				{
					LOG("Diff for opcode " << HEX(op) << ": " << assembly << ": instruction length " << opcodeCount << " != " << opcodeCountMotorola);
					disasm.disassemble(assembly, op, opB, 0, 0, 0);	// retry to help debugging
				}
			}
		}

		LOG("Disassembler Unit Tests completed");
#endif
	}
}
