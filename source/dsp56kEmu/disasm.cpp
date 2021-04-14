#if defined(_WIN32) && defined(_DEBUG)
#	define USE_MOTOROLA_UNASM
#endif

#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

#include "disasm.h"

#include <algorithm>

#include "logging.h"
#include "utils.h"

#ifdef USE_MOTOROLA_UNASM
extern "C"
{
	#include "../dsp56k/PROTO563.H"
}
#endif

namespace dsp56k
{
	const char* g_opNames[InstructionCount] = 
	{
		"abs ",
		"adc ",
		"add ",		"add ",		"add ",
		"addl ",		"addr ",
		"and ",		"and ",		"and ",		"andi ",
		"asl ",		"asl ",		"asl ",
		"asr ",		"asr ",		"asr ",
		"b",		"b",		"b",
		"bchg ",		"bchg ",		"bchg ",		"bchg ",		"bchg ",
		"bclr ",		"bclr ",		"bclr ",		"bclr ",		"bclr ",
		"bra ",		"bra ",		"bra ",
		"brclr ",	"brclr ",	"brclr ",	"brclr ",	"brclr ",
		"brk",
		"brset ",	"brset ",	"brset ",	"brset ",	"brset ",
		"bs",		"bs",		"bs",
		"bsclr ",	"bsclr ",	"bsclr ",	"bsclr ",	"bsclr ",
		"bset ",		"bset ",		"bset ",		"bset ",		"bset ",
		"bsr ",		"bsr ",		"bsr ",
		"bsset ",	"bsset ",	"bsset ",	"bsset ",	"bsset ",
		"btst ",		"btst ",		"btst ",		"btst ",		"btst ",
		"clb ",
		"clr ",
		"cmp ",		"cmp ",		"cmp ",
		"cmpm ",		"cmpu ",
		"debug ",	"debug",
		"dec ",
		"div ",
		"dmac ",
		"do ",		"do ",		"do ",		"do ",
		"doforever ",
		"dor ",		"dor ",		"dor ",		"dor ",
		"dorforever ",
		"enddo ",
		"eor ",		"eor ",		"eor ",
		"extract ",	"extract ",
		"extractu ",	"extractu ",
		"if",		"if",
		"illegal ",	"inc ",
		"insert ",	"insert ",
		"j",		"j",
		"jclr ",		"jclr ",		"jclr ",		"jclr ",		"jclr ",
		"jmp ",		"jmp ",
		"js",		"js",
		"jsclr ",	"jsclr ",	"jsclr ",	"jsclr ",	"jsclr ",
		"jset ",		"jset ",		"jset ",		"jset ",		"jset ",
		"jsr ",		"jsr ",
		"jsset ",	"jsset ",	"jsset ",	"jsset ",	"jsset ",
		"lra ",		"lra ",
		"lsl ",		"lsl ",		"lsl ",
		"lsr ",		"lsr ",		"lsr ",
		"lua ",		"lua ",
		"mac ",		"mac ",
		"maci ",
		"macsu ",
		"macr ",		"macr ",
		"macri ",
		"max ",
		"maxm ",
		"merge ",
		"move ",		"move ",
		"mover ",
		"move ",
		"movex ",	"movex ",	"movex ",		"movex ",
		"movexr ",	"movexr ",
		"movey ",	"movey ",	"movey ",		"movey ",
		"moveyr ",	"moveyr ",
		"movel ",	"movel ",
		"movexy ",
		"movec ",	"movec ",	"movec ",		"movec ",
		"movem ",	"movem ",
		"movep ",	"movep ",	"movep ",		"movep ",		"movep ",		"movep ",		"movep ",		"movep ",
		"mpy ",		"mpy ",		"mpy ",
		"mpyi ",
		"mpyr ",		"mpyr ",
		"mpyri ",
		"neg ",
		"nop ",
		"norm ",
		"normf ",
		"not ",
		"or ",		"or ",		"or ",
		"ori ",
		"pflush ",
		"pflushun ",
		"pfree ",
		"plock ",
		"plockr ",
		"punlock ",
		"punlockr ",
		"rep ",		"rep ",		"rep ",		"rep ",
		"reset ",
		"rnd ",
		"rol ",
		"ror ",
		"rti ",
		"rts ",
		"sbc ",
		"stop ",
		"sub ",		"sub ",		"sub ",
		"subl ",
		"subr ",
		"t",		"t",		"t",
		"tfr ",
		"trap ",
		"trap",
		"tst ",
		"vsl ",
		"wait ",
		"resolvecache ",
		"parallel ",
	};

	const char* g_conditionCodes[16] =	{"cc", "ge", "ne", "pl", "nn", "ec", "lc", "gt", "cs", "lt", "eq", "mi", "nr", "es", "ls", "le"};
	
	std::string hex(TWord _data)
	{
		std::stringstream ss; ss << '$' << std::hex << _data; return std::string(ss.str());
	}

	std::string immediate(TWord _data)
	{
		return std::string("#") + hex(_data);
	}

	std::string immediateLong(TWord _data)
	{
		return std::string("#>") + hex(_data);
	}

	std::string relativeAddr(int _data)
	{
		const auto a = signextend<int,24>(_data);
		return std::string(">*") + (a > 0 ? "+" : "-") + hex(std::abs(_data));
	}

	const char* aluD(bool ab)
	{
		return ab ? "b" : "a";
	}

	void disassembleDC(std::stringstream& dst, TWord op)
	{
		dst << "dc $" << HEX(op);
	}

	Disassembler::Disassembler()
	{
		m_str.reserve(1024);
	}

	void Disassembler::disassembleDC(std::string& dst, TWord op)
	{
		dsp56k::disassembleDC(m_ss, op);
		dst = m_ss.str();
	}

	int Disassembler::disassembleAlu(std::string& _dst, const OpcodeInfo& _oiAlu, TWord op)
	{
		const int res = disassembleAlu(op);
		if(res)
		{
			_dst = m_ss.str();
			return res;
		}
		disassembleDC(_dst, op);
		return 0;
	}

	const char* aluJJJ(TWord JJJ, bool ab)
	{
		switch (JJJ)
		{
			case 0:		return nullptr;
			case 1:		return aluD(!ab);	// ab = D, if D is a, the result is b and if D is b, the result is a
			case 2:		return "x";
			case 3:		return "y";
			case 4:		return "x0";
			case 5:		return "y0";
			case 6:		return "x1";
			case 7:		return "y1";
		}
		return nullptr;
	}

	const char* aluQQQ(TWord QQQ)
	{
		switch (QQQ)
		{
		case 0:	return "x0,x0";
		case 1:	return "y0,y0";
		case 2:	return "x1,x0";
		case 3:	return "y1,y0";
		case 4:	return "x0,x1";
		case 5:	return "y0,x0";
		case 6:	return "x1,x0";
		case 7:	return "y1,xx";
		}
		return nullptr;
	}

	const char* memArea(TWord _area)
	{
		return _area ? "y:" : "x:";
	}

	std::string memory(TWord S, TWord a)
	{
		return std::string(memArea(S)) + '<' + immediate(a);
	}

	std::string peripheral(TWord S, TWord a, TWord _root)
	{
		return std::string(memArea(S)) + "<<" + immediate(_root + a);
	}

	std::string peripheralQ(TWord S, TWord a)
	{
		return peripheral(S, a, 0xffffc0);
	}

	std::string peripheralP(TWord S, TWord a)
	{
		return peripheral(S, a, 0xffff80);
	}

	std::string mmmrrr(TWord mmmrrr, TWord S, TWord opB)
	{
		const char* formats[8]
		{
			"(r%d)-n%d",
			"(r%d)+n%d",
			"(r%d)-",
			"(r%d)+",
			"(r%d)",			
			"(r%d+n%d)",
			nullptr,
			"-(r%d)",
		};

		const std::string area(memArea(S));

		char temp[32];
	
		switch ((mmmrrr & 38) >> 3)
		{
		case 0x30:	// absolute address
			return area + immediateLong(opB);
		case 0x34:	// immediate data
			return area + immediateLong(opB);
		default:
			{
				const auto mmm = (mmmrrr>>3)&7;
				const auto r = mmmrrr & 7;
				const auto format = formats[mmm];
				if(!format)
					return std::string();
				sprintf(temp, format, r, r);
			}
			return area + temp;
		}
	}

	const char* condition(TWord _cccc)
	{
		return g_conditionCodes[_cccc];
	}

	const char* decodeEE(TWord _ee)
	{
		switch (_ee)
		{
		case 0:	return "mr";
		case 1:	return "ccr";
		case 2:	return "com";
		case 3:	return "eom";
		}
		return nullptr;
	}

	
	const char* decode_sss( TWord _sss )
	{
		switch( _sss )
		{
		case 2:		return "a1";
		case 3:		return "b1";
		case 4:		return "x0";
		case 5:		return "y0";
		case 6:		return "x1";
		case 7:		return "y1";
		}
		return nullptr;
	}

	std::string decode_RRR( TWord _r )
	{
		char temp[3] = {'r', '0', 0};
		temp[1] = '0' + _r;
		return temp;
	}


	std::string decode_DDDDDD( TWord _ddddd )
	{
		switch( _ddddd )
		{
		case 0x04:	return "x0";
		case 0x05:	return "x1";
		case 0x06:	return "y0";
		case 0x07:	return "y1";
		case 0x08:	return "a0";
		case 0x09:	return "b0";
		case 0x0a:	return "a2";
		case 0x0b:	return "b2";
		case 0x0c:	return "a1";
		case 0x0d:	return "b1";
		case 0x0e:	return "a";	
		case 0x0f:	return "b";	
		}

		char temp[3]{0,0,0};
		if( (_ddddd & 0x18) == 0x10 )					// r0-r7
			temp[0] = 'r';
		else if( (_ddddd & 0x18) == 0x18 )				// n0-n7
			temp[0] = 'n';
		else
			return std::string();

		temp[1] = '0' + _ddddd&0x07;

		return temp;
	}
	int Disassembler::disassembleAlu(dsp56k::TWord op)
	{
		const auto D = (op>>3) & 0x1;

		if(op>>7 == 0)
		{
			// non-multiply
			const auto kkk = op & 0x7;
			const auto JJJ = (op>>4) & 0x7;

			if(!kkk && !JJJ)
				return 0;

			const char* j = aluJJJ(JJJ, D);

			if(!j)
			{
				m_ss << aluD(D);
			}
			else
			{
				m_ss << j << "," << aluD(D);
			}

			return 1;
		}

		// multiply
		const auto QQQ = (op>>4) & 0x7;
		const char* q = aluQQQ(QQQ);
		if(!q)
			return 0;

		m_ss << q << "," << aluD(D);

		return 1;
	}

	int Disassembler::disassemble(const OpcodeInfo& oi, TWord op, TWord opB)
	{
		const auto inst = oi.m_instruction;

		switch (inst)
		{
		// immediate operand with destination ALU register
		case Add_xx:
		case And_xx:
		case Cmp_xxS2:
		case Eor_xx:
		case Or_xx:
		case Sub_xx:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto i = getFieldValue(inst, Field_iiiiii, op);
				m_ss << immediate(i) << ',' << aluD(d);
				return 1;
			}
		// immediate extension operand with destination ALU register
		case Add_xxxx:
		case And_xxxx:
		case Cmp_xxxxS2:
		case Eor_xxxx:
		case Or_xxxx:
		case Sub_xxxx:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				m_ss << immediateLong(opB) << ',' << aluD(d);
				return 2;
			}
		// bit manipulation on effective address
		case Bchg_ea:
		case Bclr_ea:
		case Bset_ea:
		case Btst_ea:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << mmmrrr(mr, S, opB);
				return 2;
			}
		case Bchg_aa:
		case Bclr_aa:
		case Bset_aa:
		case Btst_aa:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << memory(S, a);
				return 1;
			}
		case Bchg_pp:
		case Bclr_pp:
		case Bset_pp:
		case Btst_pp:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto p	= getFieldValue(inst, Field_pppppp, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << peripheralP(S, p);
				return 1;
			}
		case Bchg_qq:
		case Bclr_qq:
		case Bset_qq:
		case Btst_qq:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto q	= getFieldValue(inst, Field_qqqqqq, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << peripheralQ(S, q);
				return 1;
			}
		case Bchg_D:
		case Bclr_D:
		case Bset_D:
		case Btst_D:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << immediate(b) << ',' << decode_DDDDDD(d);
				return 1;
			}
		// Branch if bit condition is met
		case Brclr_ea:
		case Brset_ea:
		case Bsclr_ea:
		case Bsset_ea:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << mmmrrr(mr, S, opB) << ',' << relativeAddr(opB);
				return 2;
			}
		case Brclr_aa:
		case Brset_aa:
		case Bsclr_aa:
		case Bsset_aa:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << memory(S, a) << ',' << relativeAddr(opB);
				return 2;
			}
		case Brclr_pp:
		case Brset_pp:
		case Bsclr_pp:
		case Bsset_pp:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto p	= getFieldValue(inst, Field_pppppp, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << peripheralP(S, p) << ',' << relativeAddr(opB);
				return 2;
			}
		case Brclr_qq:
		case Brset_qq:
		case Bsclr_qq:
		case Bsset_qq:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto q	= getFieldValue(inst, Field_qqqqqq, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << peripheralP(S, q) << ',' << relativeAddr(opB);
				return 2;
			}
		case Andi:
			{
				const auto i = getFieldValue(inst, Field_iiiiiiii, op);
				const auto ee = getFieldValue(inst, Field_EE, op);
				m_ss << immediate(i) << ',' << decodeEE(ee);
			}
			return 1;
		case Asl_ii:
		case Asr_ii:
			{
				const auto i = getFieldValue(inst, Field_iiiiii, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(i) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Asl_S1S2D:
		case Asr_S1S2D:
			{
				const auto sss = getFieldValue(inst, Field_sss, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Bcc_xxxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_ss << condition(cccc) << ' ' << relativeAddr(opB);
			}
			break;
		case Bcc_xxx: 
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				m_ss << condition(cccc) << ' ' << relativeAddr(a);
			}
			break;
		case Bcc_Rn: 
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto r = getFieldValue(inst, Field_RRR, op);
				m_ss << condition(cccc) << ' ' << decode_RRR(r);
			}
			break;
		case Bra_xxxx: 
			break;
		case Bra_xxx: break;
		case Bra_Rn: break;
		case Brclr_S: break;
		case BRKcc: break;
		case Brset_S: break;
		case BScc_xxxx: break;
		case BScc_xxx: break;
		case BScc_Rn: break;
		case Bsclr_S: break;
		case Bsr_xxxx: break;
		case Bsr_xxx: break;
		case Bsr_Rn: break;
		case Bsset_S: break;
		case Clb: break;
		case Cmpu_S1S2: break;
		case Debug: break;
		case Debugcc: break;
		case Dec: break;
		case Div: break;
		case Dmac: break;
		case Do_ea: break;
		case Do_aa: break;
		case Do_xxx: break;
		case Do_S: break;
		case DoForever: break;
		case Dor_ea: break;
		case Dor_aa: break;
		case Dor_xxx: break;
		case Dor_S: break;
		case DorForever: break;
		case Enddo: break;
		case Extract_S1S2: break;
		case Extract_CoS2: break;
		case Extractu_S1S2: break;
		case Extractu_CoS2: break;
		case Ifcc: break;
		case Ifcc_U: break;
		case Illegal: break;
		case Inc: break;
		case Insert_S1S2: break;
		case Insert_CoS2: break;
		case Jcc_xxx: break;
		case Jcc_ea: break;
		case Jclr_ea: break;
		case Jclr_aa: break;
		case Jclr_pp: break;
		case Jclr_qq: break;
		case Jclr_S: break;
		case Jmp_ea: break;
		case Jmp_xxx: break;
		case Jscc_xxx: break;
		case Jscc_ea: break;
		case Jsclr_ea: break;
		case Jsclr_aa: break;
		case Jsclr_pp: break;
		case Jsclr_qq: break;
		case Jsclr_S: break;
		case Jset_ea: break;
		case Jset_aa: break;
		case Jset_pp: break;
		case Jset_qq: break;
		case Jset_S: break;
		case Jsr_ea: break;
		case Jsr_xxx: break;
		case Jsset_ea: break;
		case Jsset_aa: break;
		case Jsset_pp: break;
		case Jsset_qq: break;
		case Jsset_S: break;
		case Lra_Rn: break;
		case Lra_xxxx: break;
		case Lsl_ii: break;
		case Lsl_SD: break;
		case Lsr_D: break;
		case Lsr_ii: break;
		case Lsr_SD: break;
		case Lua_ea: break;
		case Lua_Rn: break;
		case Mac_S: break;
		case Maci_xxxx: break;
		case Macsu: break;
		case Macr_S: break;
		case Macri_xxxx: break;
		case Merge: break;
		case Move_Nop: break;
		case Move_xx: break;
		case Mover: break;
		case Move_ea: break;
		case Movex_ea: break;
		case Movex_aa: break;
		case Movex_Rnxxxx: break;
		case Movex_Rnxxx: break;
		case Movexr_ea: break;
		case Movexr_A: break;
		case Movey_ea: break;
		case Movey_aa: break;
		case Movey_Rnxxxx: break;
		case Movey_Rnxxx: break;
		case Moveyr_ea: break;
		case Moveyr_A: break;
		case Movel_ea: break;
		case Movel_aa: break;
		case Movexy: break;
		case Movec_ea: break;
		case Movec_aa: break;
		case Movec_S1D2: break;
		case Movec_xx: break;
		case Movem_ea: break;
		case Movem_aa: break;
		case Movep_ppea: break;
		case Movep_Xqqea: break;
		case Movep_Yqqea: break;
		case Movep_eapp: break;
		case Movep_eaqq: break;
		case Movep_Spp: break;
		case Movep_SXqq: break;
		case Movep_SYqq: break;
		case Mpy_SD: break;
		case Mpy_su: break;
		case Mpyi: break;
		case Mpyr_S1S2D: break;
		case Mpyr_SD: break;
		case Mpyri: break;
		case Nop: break;
		case Norm: break;
		case Normf: break;
		case Not: break;
		case Ori: break;
		case Pflush: break;
		case Pflushun: break;
		case Pfree: break;
		case Plock: break;
		case Plockr: break;
		case Punlock: break;
		case Punlockr: break;
		case Rep_ea: break;
		case Rep_aa: break;
		case Rep_xxx: break;
		case Rep_S: break;
		case Reset: break;
		case Rti: break;
		case Rts: break;
		case Sbc: break;
		case Tcc_S1D1: break;
		case Tcc_S1D1S2D2: break;
		case Tcc_S2D2: break;
		case Trap: break;
		case Trapcc: break;
		case Vsl: break;
		case Wait: break;
		case ResolveCache: break;
		case Parallel: break;
		case InstructionCount: break;
		default: 
			return 0;
		}
		return 1;
	}

	int Disassembler::disassembleParallelMove(const OpcodeInfo& oi, TWord _op, TWord _opB)
	{
		return disassemble(oi, _op, _opB);
	}

	int Disassembler::disassembleNonParallel(const OpcodeInfo& oi, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
		return disassemble(oi, op, opB);
	}

	int Disassembler::disassemble(std::string& dst, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
		m_ss.str("");
		m_ss.clear();

		auto finalize = [&](int res)
		{
			dst = m_ss.str();
			return res;
		};

		if(!op)
		{
			dst = g_opNames[Nop];
			return 1;
		}

		if(Opcodes::isNonParallelOpcode(op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);
			if(oi)
			{
				m_ss << g_opNames[oi->m_instruction];
				const auto res = disassembleNonParallel(*oi, op, opB, sr, omr);
				if(!res)
				{
					disassembleDC(dst, op);
					return 0;
				}

				return finalize(res);
			}

			disassembleDC(dst, op);
			return 0;
		}

		const auto* oiMove = m_opcodes.findParallelMoveOpcodeInfo(op);

		if(!oiMove)
		{
			disassembleDC(dst, op);
			return 0;
		}

		const OpcodeInfo* oiAlu = nullptr;

		if(op & 0xff)
		{
			oiAlu = m_opcodes.findParallelAluOpcodeInfo(op);
			if(!oiAlu)
			{
				disassembleDC(dst, op);
				return 0;
			}
		}
		else if(!oiAlu)
		{
			m_ss << g_opNames[oiMove->m_instruction];
			const int resMove = disassembleParallelMove(*oiMove, op, opB);
			if(resMove)
				return finalize(resMove);
			disassembleDC(dst, op);
			return 0;
		}

		switch (oiMove->m_instruction)
		{
		case Move_Nop:
			{
				m_ss << g_opNames[oiAlu->m_instruction];
				const auto resAlu = disassembleAlu(op & 0xff);
				if(resAlu)
					return finalize(resAlu);
				disassembleDC(dst, op);
			}
			return 0;
		default:
			{
				m_ss << g_opNames[oiAlu->m_instruction];

				const auto resAlu = disassembleAlu(op & 0xff);
				const auto resMove = disassembleParallelMove(*oiMove, op, opB);
				return finalize(std::max(resMove, resAlu));
			}
		}
	}
	
	uint32_t disassemble(char* dst, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
#ifdef USE_MOTOROLA_UNASM
		unsigned long ops[3] = {op, opB, 0};
		const int ret = dspt_unasm_563( ops, dst, sr, omr, nullptr );
//		return ret;
#endif
		static Disassembler disassembler;

		std::string assembly;
		const int res = disassembler.disassemble(assembly, op, opB, sr, omr);
		strcpy(dst, assembly.c_str());
		return res;
	}
}
