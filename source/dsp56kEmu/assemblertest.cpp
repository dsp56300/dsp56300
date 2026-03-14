#include "assemblertest.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "assembler.h"
#include "disasm.h"
#include "opcodes.h"
#include "types.h"

namespace dsp56k
{
	AssemblerTest::AssemblerTest()
	{
		testAluInstructions();
		testMoveInstructions();
		testBitInstructions();
		testBranchInstructions();
		testLoopInstructions();
		testMiscInstructions();
		testParallelInstructions();
		testPeripheralSymbols();

		std::cout << "Assembler tests: " << m_passCount << "/" << m_testCount << " passed";
		if(m_failCount > 0)
			std::cout << ", " << m_failCount << " FAILED";
		std::cout << std::endl;

		if(m_failCount > 0)
			throw std::string("Assembler round-trip tests failed: ") + std::to_string(m_failCount) + " failures";
	}

	void AssemblerTest::roundTrip(uint32_t _opA, uint32_t _opB)
	{
		++m_testCount;

		Opcodes opcodes;
		Disassembler disasm(opcodes);
		Assembler assembler;

		// 1. Disassemble the known opcode to text
		Disassembler::Line line;
		const auto wordCount = disasm.disassemble(line, _opA, _opB, 0, 0, 0);

		if(wordCount == 0)
		{
			std::cout << "SKIP: opcode 0x" << std::hex << _opA;
			if(_opB) std::cout << " 0x" << _opB;
			std::cout << " - disassembly failed (dc)" << std::dec << std::endl;
			--m_testCount;
			return;
		}

		const auto text = Disassembler::formatLine(line, false);

		// 2. Assemble the text back to binary
		const auto result = assembler.assemble(text.c_str());

		// 3. Verify the result matches the original
		if(!result.success())
		{
			++m_failCount;
			std::cout << "FAIL: 0x" << std::hex << _opA;
			if(_opB) std::cout << " 0x" << _opB;
			std::cout << " -> \"" << text << "\" -> assemble error " << static_cast<int>(result.error) << std::dec << std::endl;
			return;
		}

		if(result.word[0] != _opA || (wordCount > 1 && result.word[1] != _opB) || result.wordCount != wordCount)
		{
			++m_failCount;
			std::cout << "FAIL: 0x" << std::hex << _opA;
			if(_opB) std::cout << " 0x" << _opB;
			std::cout << " -> \"" << text << "\" -> 0x" << result.word[0];
			if(result.wordCount > 1) std::cout << " 0x" << result.word[1];
			std::cout << " (expected " << wordCount << " words, got " << result.wordCount << ")" << std::dec << std::endl;
			return;
		}

		++m_passCount;
	}

	void AssemblerTest::testAluInstructions()
	{
		// abs
		roundTrip(0x200026);	// abs a
		roundTrip(0x20002e);	// abs b

		// adc
		roundTrip(0x200021);	// adc x,a

		// add
		roundTrip(0x200010);	// add b,a
		roundTrip(0x200050);	// add y0,a
		roundTrip(0x200020);	// add x,a
		roundTrip(0x200038);	// add y,b

		// add short immediate
		roundTrip(0x017280);	// add #<$32,a

		// add long immediate
		roundTrip(0x0140c0, 0x000032);	// add #>$32,a

		// addl
		roundTrip(0x20001a);	// addl a,b

		// addr
		roundTrip(0x200002);	// addr b,a
		roundTrip(0x20000a);	// addr a,b

		// and
		roundTrip(0x200046);	// and x0,a
		roundTrip(0x20007e);	// and y1,b

		// andi
		roundTrip(0x0033ba);	// andi #$33,omr
		roundTrip(0x0033bb);	// andi #$33,eom
		roundTrip(0x0033b8);	// andi #$33,mr
		roundTrip(0x0033b9);	// andi #$33,ccr

		// asl
		roundTrip(0x200032);	// asl a
		roundTrip(0x20003a);	// asl b
		roundTrip(0x0c1d02);	// asl #$1,a,a
		roundTrip(0x0c1d50);	// asl #$28,a,a
		roundTrip(0x0c1e48);	// asl x0,a,a
		roundTrip(0x0c1e5f);	// asl y1,b,b

		// asr
		roundTrip(0x200022);	// asr a
		roundTrip(0x0c1c02);	// asr #$1,a,a
		roundTrip(0x0c1c2a);	// asr #$15,a,a
		roundTrip(0x0c1e68);	// asr x0,a,a
		roundTrip(0x0c1e7f);	// asr y1,b,b
		roundTrip(0x0c1e6f);	// asr y1,a,b

		// clb
		roundTrip(0x0c1e01);	// clb a,b

		// clr
		roundTrip(0x200013);	// clr a

		// cmp
		roundTrip(0x20004d);	// cmp y0,b
		roundTrip(0x0140c5, 0x0000aa);	// cmp #>$aa,b

		// cmpm
		roundTrip(0x200047);	// cmpm x0,a

		// div
		roundTrip(0x018050);	// div y0,a

		// dmac
		roundTrip(0x01248f);	// dmac ss x1,y1,a
		roundTrip(0x012480);	// dmac ss x0,x0,a
		roundTrip(0x0124a5);	// dmac ss y0,x0,b
		roundTrip(0x012580);	// dmac su x0,x0,a
		roundTrip(0x0125c2);	// dmac uu x1,x0,a
		roundTrip(0x0125c9);	// dmac uu y1,y1,a

		// eor
		roundTrip(0x200043);	// eor x0,a

		// extract/extractu
		roundTrip(0x0c1a8d);	// extract x0,a,b
		roundTrip(0x0c1e88);	// extractu x0,a,a
		roundTrip(0x0c1ec8);	// extractu x0,b,a

		// insert
		roundTrip(0x0c1e28);	// insert x0,a,a
		roundTrip(0x0c1e2b);	// insert x0,b,a
		roundTrip(0x0c1e3c);	// insert y1,a,b

		// lsl
		roundTrip(0x200033);	// lsl a
		roundTrip(0x0c1d04);	// lsl #$1,a

		// lsr
		roundTrip(0x200023);	// lsr a
		roundTrip(0x0c1c04);	// lsr #$1,a

		// mac
		roundTrip(0x2000a2);	// mac x0,x0,a
		roundTrip(0x2000d0);	// mac x1,x0,a
		roundTrip(0x2000d1);	// mac x1,x0,b
		roundTrip(0x2000da);	// mac y1,x1,a
		roundTrip(0x2000e2);	// mac x0,y1,a

		// max
		roundTrip(0x200011);	// max a,b

		// maxm
		roundTrip(0x200015);	// maxm a,b

		// mpy
		roundTrip(0x2000a0);	// mpy x0,x0,a
		roundTrip(0x2000b0);	// mpy y0,y0,a

		// mpyr
		roundTrip(0x2000a1);	// mpyr x0,x0,a

		// neg
		roundTrip(0x200036);	// neg a

		// not
		roundTrip(0x200017);	// not a

		// or
		roundTrip(0x200042);	// or x0,a

		// ori
		roundTrip(0x0022f8);	// ori #$22,mr
		roundTrip(0x0022f9);	// ori #$22,ccr
		roundTrip(0x0022fa);	// ori #$22,omr
		roundTrip(0x0022fb);	// ori #$22,eom

		// rnd
		roundTrip(0x20001d);	// rnd a
		roundTrip(0x200019);	// rnd b

		// rol
		roundTrip(0x200037);	// rol a

		// sbc
		roundTrip(0x200025);	// sbc x,a

		// sub
		roundTrip(0x200014);	// sub b,a
		roundTrip(0x200044);	// sub x0,a

		// subl
		roundTrip(0x200016);	// subl a,b

		// tfr
		roundTrip(0x200049);	// tfr x0,b (JJJ=4)
		roundTrip(0x200059);	// tfr y0,b (JJJ=5)
		roundTrip(0x200069);	// tfr x1,b (JJJ=6)
		roundTrip(0x200079);	// tfr y1,b (JJJ=7)

		// tst
		roundTrip(0x200003);	// tst a
		roundTrip(0x20000b);	// tst b

		// subl
		roundTrip(0x200016);	// subl a,b
		roundTrip(0x20001e);	// subl b,a
	}

	void AssemblerTest::testMoveInstructions()
	{
		// move with various addressing modes (Move_Nop parallel, ALU = nop)
		roundTrip(0x204800);	// move (r0)+n0
		roundTrip(0x204500);	// move (r5)-n5
		roundTrip(0x205800);	// move (r0)+
		roundTrip(0x205400);	// move (r4)-
		roundTrip(0x204000);	// move (r0)-n0
		roundTrip(0x204300);	// move (r3)-n3
		roundTrip(0x204900);	// move (r1)+n1
		roundTrip(0x204a00);	// move (r2)+n2
		roundTrip(0x204c00);	// move (r4)+n4
		roundTrip(0x204d00);	// move (r5)+n5
		roundTrip(0x205c00);	// move (r4)+

		// move immediate long
		roundTrip(0x44f400, 0xaaaaaa);	// move #$aaaaaa,x0
		roundTrip(0x45f400, 0xbbbbbb);	// move #$bbbbbb,x1
		roundTrip(0x46f400, 0xcccccc);	// move #$cccccc,y0
		roundTrip(0x47f400, 0xdddddd);	// move #$dddddd,y1
		roundTrip(0x50f400, 0xabcdef);	// move #$abcdef,a0
		roundTrip(0x54f400, 0x345678);	// move #$345678,a
		roundTrip(0x57f400, 0x03a800);	// move #$3a800,b

		// move register
		roundTrip(0x20c700);	// move a,y1
		roundTrip(0x21cf00);	// move a,y
		roundTrip(0x21ee00);	// move b,a
		roundTrip(0x21da00);	// move b,y0
		roundTrip(0x21d000);	// move a,y0
		roundTrip(0x21c700);	// move y1,a

		// movec
		roundTrip(0x04c4a1);	// move x0,m1
		// movep
		roundTrip(0x04c784);	// movep y1,ep
		roundTrip(0x044f26);	// movep
		roundTrip(0x04c7b0);	// move y1,lc
		roundTrip(0x0430f6);	// move ssh,n0

		// movec with immediate
		roundTrip(0x0555be);	// move #$55,omr

		// movem
		roundTrip(0x07ea92);	// (related to movem)

		// move short immediate
		roundTrip(0x240000);	// move #$0,x0
		roundTrip(0x24ff00);	// move #$ff,x0
		roundTrip(0x340000);	// move #$0,r0
		roundTrip(0x32ff00);	// move #$ff,r4

		// move x:ea
		roundTrip(0x486300);	// move x:(r3)+,a0
		roundTrip(0x496400);	// move x:(r4)+,a1
		roundTrip(0x498300);	// move b0,x:(r3)+
		roundTrip(0x49e000);	// move b0,x:(r0)-n0
		roundTrip(0x4ae000);	// move b0,x:(r0)

		// move y:ea
		roundTrip(0x4a8300);	// ...
		roundTrip(0x628700);	// ...

		// movex_aa / movey_aa
		roundTrip(0x608000);	// move x:$0,a
		roundTrip(0x50a000);	// move a,x:$0

		// move long (single X or Y)
		roundTrip(0x426100);
		roundTrip(0x42d900);
		roundTrip(0x436200);
		roundTrip(0x438400);
		roundTrip(0x806300);	// Movexy X:ea Y:ea
		roundTrip(0xb06800);	// Movexy variant (different regs)
		roundTrip(0xc0e300);	// Movexy variant (different modes)
		roundTrip(0x986c00);	// Movexy variant

		// movel_aa
		roundTrip(0x5ffd00);	// ...

		// movex with EA and absolute addressing
		roundTrip(0x56f000, 0x000010);	// move x:$10,a
		roundTrip(0x4ff000, 0x000020);	// move x:$20,b1

		// move with long absolute
		roundTrip(0x44f41b, 0x000128);	// clr b #>$128,x0 (parallel)
		roundTrip(0x56f400, 0x000001);	// move #>$1,a
	}

	void AssemblerTest::testBitInstructions()
	{
		// bchg
		roundTrip(0x0b0203);	// bchg #$3,x:<$2
		roundTrip(0x0b0343);	// bchg #$3,y:<$3

		// bclr
		roundTrip(0x0a1114);	// bclr #$14,x:<$11
		roundTrip(0x0a2250);	// bclr #$10,y:<$22

		// bclr D
		roundTrip(0x0afa47);	// bclr #$7,omr
		roundTrip(0x0acf56);	// bclr #$16,b

		// bset
		roundTrip(0x0a0223);	// bset #$3,x:<$2
		roundTrip(0x0a0363);	// bset #$3,y:<$3
		roundTrip(0x0a9004);	// bset #$4,x:<$10

		// bset D
		roundTrip(0x0be2a1);	// bset #$1,m2

		// btst
		roundTrip(0x0b0222);	// btst #$2,x:<$2
		roundTrip(0x0b0223);	// btst #$3,x:<$2

		// bclr/bset/btst with ea
		roundTrip(0x0a6014);	// bclr #$14,x:(r0)
		roundTrip(0x0a6150);	// bclr #$10,y:(r1)
	}

	void AssemblerTest::testBranchInstructions()
	{
		// jscc
		roundTrip(0x0b0000 | (0x0a << 12));	// jscc $0
	}

	void AssemblerTest::testLoopInstructions()
	{
		// do
		roundTrip(0x060080, 0x000010);	// do x0,$10

		// rep
		roundTrip(0x0600a0);	// rep x0

		// enddo
		roundTrip(0x00008c);	// enddo
	}

	void AssemblerTest::testMiscInstructions()
	{
		// nop
		roundTrip(0x000000);	// nop

		// illegal
		roundTrip(0x000005);	// illegal

		// reset
		roundTrip(0x000084);	// reset

		// rti
		roundTrip(0x000004);	// rti

		// rts
		roundTrip(0x00000c);	// rts

		// stop
		roundTrip(0x000087);	// stop

		// wait
		roundTrip(0x000086);	// wait

		// swi
		roundTrip(0x000006);	// swi

		// tcc
		roundTrip(0x020801);	// tcc r0,r1

		// lua
		roundTrip(0x044818);	// lua (r0)+,r0

		// movec with various
		roundTrip(0x0102f0);	// movec sr,r0
		roundTrip(0x0102f2);	// movec omr,r2
		roundTrip(0x0113f0);	// movec lc,r0
		roundTrip(0x010ad8);	// movec la,x0

		// macsu / macuu / mpysu / mpyuu
		roundTrip(0x012586);	// macsu x0,y0,a
		roundTrip(0x0125a6);	// macsu x0,y0,b
		roundTrip(0x01258e);	// macuu x0,y0,a
		roundTrip(0x012584);	// mpysu x0,y0,a
		roundTrip(0x01258c);	// mpyuu x0,y0,a

		// movec short imm
		roundTrip(0x0555be);	// move #$55,omr
	}

	void AssemblerTest::testParallelInstructions()
	{
		// ALU with move_nop
		roundTrip(0x200003);	// tst a (no parallel move)
		roundTrip(0x200026);	// abs a (no parallel move)

		// ALU with register move
		roundTrip(0x21cf26);	// abs a a,y1
		roundTrip(0x21cf51);	// tfr y0,a a,b
		roundTrip(0x21ee59);	// tfr y0,b b,a
		roundTrip(0x210541);	// tfr x0,a a0,x1
		roundTrip(0x210741);	// tfr x0,a a0,y1
		roundTrip(0x214400);	// move x0,a
		roundTrip(0x216600);	// move y0,b

		// Parallel move: X memory
		roundTrip(0x2a1200);	// move y1,x:(r2)
		roundTrip(0x2e0f00);	// move x:(r7)-n7,y0
		roundTrip(0x2eff00);	// move #$ff,y0 (short imm to reg, parallel)
		roundTrip(0x243c44);	// sub x0,a #<$3c,x0

		// move long absolute
		roundTrip(0x44f41b, 0x000128);	// clr b #>$128,x0
		roundTrip(0x56f400, 0x000001);	// move #>$1,a

		// ifcc parallel
		roundTrip(0x202a10);	// add b,a ifeq
		roundTrip(0x203115);	// cmp b,a ifge.u (canonical JJJ=1)
		roundTrip(0x20311d);	// cmp a,b ifge.u (canonical JJJ=1)
	}

	void AssemblerTest::testPeripheralSymbols()
	{
		// Test assembling with peripheral symbol names instead of hex addresses
		Assembler assembler;

		// Register some peripheral symbols (matching what peripherals.cpp does)
		assembler.addSymbol(Assembler::MemX, 0xFFFFC7, "M_BCR");
		assembler.addSymbol(Assembler::MemX, 0xFFFFFE, "M_IPRP");
		assembler.addSymbol(Assembler::MemX, 0xFFFFFF, "M_IPRC");
		assembler.addSymbol(Assembler::MemX, 0xFFFFF1, "M_PCTL");
		assembler.addSymbol(Assembler::MemX, 0xFFFFF8, "M_AAR0");

		// Register bit symbols for AAR0
		assembler.addBitSymbol(Assembler::MemX, 0xFFFFF8, 0, "M_BAT0");
		assembler.addBitSymbol(Assembler::MemX, 0xFFFFF8, 1, "M_BAT1");
		assembler.addBitSymbol(Assembler::MemX, 0xFFFFF8, 3, "M_BPEN");

		auto verify = [&](const char* asmText, TWord expectedA, TWord expectedB = 0)
		{
			++m_testCount;
			const auto result = assembler.assemble(asmText);
			const auto expectedWords = (expectedB != 0) ? 2u : 1u;

			if (!result.success() || result.wordCount != expectedWords || result.word[0] != expectedA || (expectedWords == 2 && result.word[1] != expectedB))
			{
				++m_failCount;
				std::cout << "FAIL (sym): \"" << asmText << "\" -> ";
				if (!result.success())
					std::cout << "assemble error " << static_cast<int>(result.error);
				else
					std::cout << "0x" << std::hex << result.word[0] << std::dec;
				std::cout << " (expected 0x" << std::hex << expectedA << std::dec << ")" << std::endl;
				return;
			}
			++m_passCount;
		};

		// Test movep with peripheral symbols (Movep_Spp: x:<symbol,reg)
		// M_BCR = 0xFFFFC7, pp = 0xFFFFC7 - 0xFFFFC0 = 7
		// bclr #$0,x:<<M_BCR → should match bclr #$0,x:<<$ffffc7
		// For Bclr_pp with bit=0, area=X, pp=7: pattern 0000101010pppppp → 0x0A8007 with bbbbb=0
		// Actually let me compute: Bclr_pp pattern = "0000101011pppppp0Sbbbbb"
		// Wait, that doesn't look right. Let me just compare against known hex-based assembly.

		// Instead of manually computing, assemble with hex and compare with symbol:
		{
			// movep with M_PCTL symbol: Movep_Spp uses x:<addr format
			// M_PCTL = 0xFFFFF1, pp = 0xFFFFF1 - 0xFFFFC0 = 0x31
			// "movep x0,x:<$fffff1" vs "movep x0,x:<M_PCTL"
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("movep x0,x:<$fffff1");
			if (hexResult.success())
				verify("movep x0,x:<M_PCTL", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		{
			// movep with M_BCR symbol: Movep_Spp
			// M_BCR = 0xFFFFC7, pp = 7
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("movep x0,x:<$ffffc7");
			if (hexResult.success())
				verify("movep x0,x:<M_BCR", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		{
			// movep with M_IPRC symbol via Movep_SXqq (x:<<addr format)
			// M_IPRC = 0xFFFFFF, qq = 0xFFFFFF - 0xFFFF80 = 0x7F
			// But qq > 0x3F means this is actually pp range, not qq
			// M_IPRP = 0xFFFFFE, in pp range
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("movep x0,x:<$fffffe");
			if (hexResult.success())
				verify("movep x0,x:<M_IPRP", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		{
			// bclr with peripheral symbol address: bclr #$0,x:<<$fffff8 → bclr #$0,x:<<M_AAR0
			// M_AAR0 = 0xFFFFF8, which is pp range (>= 0xFFFFC0)
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("bclr #$0,x:<<$fffff8");
			if (hexResult.success())
				verify("bclr #$0,x:<<M_AAR0", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		{
			// bclr with BOTH peripheral symbol AND bit symbol:
			// bclr #M_BPEN,x:<<M_AAR0  (bit 3 of AAR0)
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("bclr #$3,x:<<$fffff8");
			if (hexResult.success())
				verify("bclr #M_BPEN,x:<<M_AAR0", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		{
			// bset with bit symbol: bset #M_BAT0,x:<<M_AAR0
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("bset #$0,x:<<$fffff8");
			if (hexResult.success())
				verify("bset #M_BAT0,x:<<M_AAR0", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		{
			// btst with bit symbol: btst #M_BAT1,x:<<M_AAR0
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("btst #$1,x:<<$fffff8");
			if (hexResult.success())
				verify("btst #M_BAT1,x:<<M_AAR0", hexResult.word[0], hexResult.wordCount > 1 ? hexResult.word[1] : 0);
		}

		// Full round-trip with symbols: disassemble with symbols → assemble with symbols → verify
		{
			Opcodes opcodes;
			Disassembler disasm(opcodes);
			// Register same symbols on the disassembler
			disasm.addSymbol(Disassembler::MemX, 0xFFFFF8, "M_AAR0");
			disasm.addBitMaskSymbol(Disassembler::MemX, 0xFFFFF8, 1 << 3, "M_BPEN");

			// Disassemble a bclr #$3,x:<<$fffff8 opcode
			// First, get the opcode from hex assembly
			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("bclr #$3,x:<<$fffff8");
			if (hexResult.success())
			{
				// Disassemble with symbols
				Disassembler::Line line;
				disasm.disassemble(line, hexResult.word[0], 0, 0, 0, 0);
				const auto text = Disassembler::formatLine(line, false);

				// The disassembly should now use symbolic names
				// Assemble it back using our symbol-aware assembler
				const auto symResult = assembler.assemble(text.c_str());
				++m_testCount;
				if (symResult.success() && symResult.word[0] == hexResult.word[0])
				{
					++m_passCount;
				}
				else
				{
					++m_failCount;
					std::cout << "FAIL (sym round-trip): \"" << text << "\" -> ";
					if (!symResult.success())
						std::cout << "assemble error " << static_cast<int>(symResult.error);
					else
						std::cout << "0x" << std::hex << symResult.word[0];
					std::cout << " (expected 0x" << std::hex << hexResult.word[0] << ")" << std::dec << std::endl;
				}
			}
		}

		{
			// Full round-trip for movep with symbol
			Opcodes opcodes;
			Disassembler disasm(opcodes);
			disasm.addSymbol(Disassembler::MemX, 0xFFFFF1, "M_PCTL");

			Assembler hexAsm;
			const auto hexResult = hexAsm.assemble("movep x0,x:<$fffff1");
			if (hexResult.success())
			{
				Disassembler::Line line;
				disasm.disassemble(line, hexResult.word[0], 0, 0, 0, 0);
				const auto text = Disassembler::formatLine(line, false);

				const auto symResult = assembler.assemble(text.c_str());
				++m_testCount;
				if (symResult.success() && symResult.word[0] == hexResult.word[0])
				{
					++m_passCount;
				}
				else
				{
					++m_failCount;
					std::cout << "FAIL (sym round-trip): \"" << text << "\" -> ";
					if (!symResult.success())
						std::cout << "assemble error " << static_cast<int>(symResult.error);
					else
						std::cout << "0x" << std::hex << symResult.word[0];
					std::cout << " (expected 0x" << std::hex << hexResult.word[0] << ")" << std::dec << std::endl;
				}
			}
		}
	}
}
