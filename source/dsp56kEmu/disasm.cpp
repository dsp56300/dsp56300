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

	std::string absAddr(int _data)
	{
		return std::string(">") + hex(_data);
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
		if(_area > 1)
			return "";
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
	
		switch ((mmmrrr & 0x38))
		{
		case 0x30:	// absolute address
			return absAddr(opB);
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

	const char* decode_alu_GGG( TWord _ggg, bool D )
	{
		switch( _ggg )
		{
		case 0:		return aluD(!D);
		case 1:		return "?";
		case 2:		return "?";
		case 3:		return "?";
		case 4:		return "x0";
		case 5:		return "y0";
		case 6:		return "x1";
		case 7:		return "y1";
		}
		return "?";
	}
	
	std::string decode_RRR(TWord _r)
	{
		char temp[3] = "r0";
		temp[1] = '0' + _r;
		return temp;
	}

	std::string decode_RRR( TWord _r, TWord shortDisplacement)
	{
		const auto displacement = signextend<int,24>(shortDisplacement);
		char temp[16] = "(r0)";
		sprintf(temp, "(r%d+%s)", _r, immediate(displacement).c_str());
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

		temp[1] = '0' + (_ddddd&0x07);

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

	const char* decode_JJ_read( TWord jj )
	{
		switch( jj )
		{
		case 0: return "x0";
		case 1: return "y0";
		case 2: return "x1";
		case 3: return "y1";
		}
		return "?";
	}

	const char* decode_QQQQ_read( TWord _qqqq )
	{
		switch( _qqqq )
		{
		case 0:  return "x0,x0";
		case 1:  return "y0,y0";
		case 2:  return "x1,x0";
		case 3:  return "y1,y0";
		case 4:  return "x0,y1";
		case 5:  return "y0,x0";
		case 6:  return "x1,y0";
		case 7:  return "y1,x1";
		case 8:  return "x1,x1";
		case 9:  return "y1,y1";
		case 10: return "x0,x1";
		case 11: return "y0,y1";
		case 12: return "y1,x0";
		case 13: return "x0,y0";
		case 14: return "y0,x1";
		case 15: return "y1,y1";
		}
		return "?,?";
	}

	const char* decode_QQ_read( TWord _qq )
	{
		switch( _qq )
		{
		case 0:		return "y1";
		case 1:		return "x0";
		case 2:		return "y0";
		case 3:		return "x1";
		}
		return "?";
	}

	const char* decode_qq_read( TWord _qq )
	{
		switch( _qq )
		{
		case 0:		return "x0";
		case 1:		return "y0";
		case 2:		return "x1";
		case 3:		return "y1";
		}
		return "?";
	}

	int Disassembler::disassemble(const OpcodeInfo& oi, TWord op, TWord opB)
	{
		const auto inst = oi.m_instruction;

		switch (inst)
		{
		// ALU operations without any other variables
		case Dec:
		case Inc:
			{
				const auto d = getFieldValue(inst, Field_d, op);				
				m_ss << aluD(d);
			}
			return 1;			
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
				m_ss << immediate(b) << ',' << peripheralQ(S, q) << ',' << relativeAddr(opB);
				return 2;
			}
		case Brclr_S:
		case Brset_S:
		case Bsclr_S:
		case Bsset_S:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << immediate(b) << ',' << decode_DDDDDD(d) << ',' << relativeAddr(opB);
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
		case Insert_S1S2:
			{
				const auto sss = getFieldValue(inst, Field_sss, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Bcc_xxxx:
		case BScc_xxxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_ss << condition(cccc) << ' ' << relativeAddr(opB);
			}
			return 2;
		case Bcc_xxx: 
		case BScc_xxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				m_ss << condition(cccc) << ' ' << relativeAddr(a);
			}
			return 1;
		case Bcc_Rn: 
		case BScc_Rn:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto r = getFieldValue(inst, Field_RRR, op);
				m_ss << condition(cccc) << ' ' << decode_RRR(r);
			}
			return 1;
		case Bra_xxxx: 
		case Bsr_xxxx:
			m_ss << relativeAddr(opB);
			return 2;
		case Bra_xxx: 
		case Bsr_xxx:
			{
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				m_ss << relativeAddr(a);
			}
			return 1;
		case Bra_Rn: 
		case Bsr_Rn:
			{
				const auto r = getFieldValue(inst, Field_RRR, op);
				m_ss << decode_RRR(r);
			}
			return 1;
		case BRKcc:
		case Debugcc:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_ss << condition(cccc);
			}
			return 1;
		case Clb:
			{
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Cmpu_S1S2:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto ggg = getFieldValue(inst, Field_ggg, op);
				m_ss << decode_alu_GGG(ggg, d) << ',' << aluD(d);
			}
			return 1;
		case Div:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto jj = getFieldValue(inst, Field_JJ, op);
				m_ss << decode_JJ_read(jj) << ',' << aluD(d);
			}
			return 1;
		case Dmac:
			{
				const bool srcUnsigned	= getFieldValue<Dmac,Field_S>(op);
				const bool dstUnsigned	= getFieldValue<Dmac,Field_s>(op);
				const bool ab			= getFieldValue<Dmac,Field_d>(op);
				const bool negate		= getFieldValue<Dmac,Field_k>(op);

				const TWord qqqq		= getFieldValue<Dmac,Field_QQQQ>(op);

				m_ss << (srcUnsigned ? 'u' : 's');
				m_ss << (dstUnsigned ? 'u' : 's');
				m_ss << ' ';
				if(negate)
					m_ss << '-';
				m_ss << decode_QQQQ_read(qqqq) << ',' << aluD(ab);
			}
			return 1;
		case Do_ea: 
		case Dor_ea:
			{
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << mmmrrr(mr, S, opB) << ',' << relativeAddr(opB + 1);
			}
			return 2;
		case Do_aa: 
		case Dor_aa:
			{
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << memory(S, a) << ',' << relativeAddr(opB + 1);
			}
			return 1;
		case Do_xxx: 
		case Dor_xxx:
			{
				const auto a	= getFieldValue(inst, Field_hhhh, Field_iiiiiiii, op);
				m_ss << immediate(a) << ',' << relativeAddr(opB + 1);
			}
			return 1;
		case Do_S: 
		case Dor_S:
			{
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << decode_DDDDDD(d) << ',' << relativeAddr(opB + 1);
			}
			return 1;
		case Extract_S1S2:
		case Extractu_S1S2:
			{
				const auto sss = getFieldValue(inst, Field_SSS, op);
				const auto s = getFieldValue(inst, Field_s, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Extract_CoS2: 
		case Extractu_CoS2:
		case Insert_CoS2:
			{
				const auto s = getFieldValue(inst, Field_s, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(opB) << ',' << aluD(s) << ',' << aluD(d);				
			}
			return 2;
		case Ifcc:
		case Ifcc_U:
			{				
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_ss << condition(cccc);
			}
			return 1;
		case Jcc_xxx: 
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = getFieldValue(inst, Field_aaaaaaaaaaaa, op);
				m_ss << condition(cccc) << ' ' << absAddr(a);
			}
			return 1;
		case Jcc_ea: 
		case Jscc_ea:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto dd = getFieldValue(inst, Field_MMM, Field_RRR, op);
				m_ss << condition(cccc) << ' ' << decode_DDDDDD(dd);
			}
			return 1;
		case Jclr_ea:
		case Jset_ea:
		case Jsclr_ea:
		case Jsset_ea:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				m_ss << immediate(b) << ',' << mmmrrr(mr, 2, opB) << ',' << absAddr(opB);
			}
			return 2;
		case Jclr_aa:
		case Jset_aa:
		case Jsclr_aa:
		case Jsset_aa:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << memory(S, a) << ',' << absAddr(opB);
			}
			return 2;
		case Jclr_pp:
		case Jsclr_pp:
		case Jset_pp:
		case Jsset_pp:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto p	= getFieldValue(inst, Field_pppppp, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << peripheralP(S, p) << ',' << absAddr(opB);
			}
			return 2;
		case Jclr_qq:
		case Jsclr_qq:
		case Jset_qq:
		case Jsset_qq:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto q	= getFieldValue(inst, Field_qqqqqq, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << immediate(b) << ',' << peripheralQ(S, q) << ',' << absAddr(opB);
			}
			return 2;
		case Jclr_S:
		case Jsclr_S:
		case Jset_S:
		case Jsset_S:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << immediate(b) << ',' << decode_DDDDDD(d) << ',' << absAddr(opB);
			}
			return 2;
		case Jmp_ea: 
		case Jsr_ea:
			{
				const auto mr = getFieldValue(inst, Field_MMM, Field_RRR, op);
				m_ss << mmmrrr(mr, 2, opB);
			}
			return 2;
		case Jmp_xxx:
		case Jsr_xxx:
			m_ss << absAddr(opB);
			return 1;

		case Lra_Rn:
			{
				const auto rr = getFieldValue(inst, Field_RRR, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				m_ss << decode_RRR(rr) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Lra_xxxx: 
			{
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				m_ss << relativeAddr(opB) << ',' << decode_DDDDDD(dd);
			}
			return 2;
		case Lsl_ii: 
		case Lsr_ii:
			{
				const auto i = getFieldValue(inst, Field_iiiii, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(i) << ',' << aluD(d);
			}
			return 1;
		case Lsl_SD:
		case Lsr_SD:
			{
				const auto ss = getFieldValue(inst, Field_sss, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Lua_ea:
			{
				const auto mr = getFieldValue(inst, Field_MM, Field_RRR, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);				
				m_ss << mmmrrr(mr, 2, opB) << ',' << decode_DDDDDD(dd);
			}
			// TODO: every function that uses mmmrrr has a dynamic length of either 1 or 2
			return 2;
		case Lua_Rn: 
			{
				const auto aa = getFieldValue(inst, Field_aaa, Field_aaaaa, op);
				const auto rr = getFieldValue(inst, Field_RRR, op);				
				const auto dd = getFieldValue(inst, Field_ddddd, op);				
				m_ss << decode_RRR(rr, aa) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Mac_S:
		case Mpy_SD:
			{
				const int sssss		= getFieldValue(inst,Field_sssss,op);
				const TWord QQ		= getFieldValue(inst,Field_QQ,op);
				const bool ab		= getFieldValue(inst,Field_d,op);
				const bool negate	= getFieldValue(inst,Field_k,op);

				if(negate)
					m_ss << '-';
				m_ss << decode_QQ_read(QQ) << ',' << immediate(sssss) << ',' << aluD(ab);
			}
			break;
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
		case Mpy_su: break;
		case Mpyi: break;
		case Mpyr_S1S2D: break;
		case Mpyr_SD: break;
		case Mpyri: break;
		case Norm: break;
		case Normf: break;
		case Not: break;
		case Ori: break;
		case Plock: break;
		case Punlock: break;
		case Rep_ea: break;
		case Rep_aa: break;
		case Rep_xxx: break;
		case Rep_S: break;
		case Sbc: break;
		case Tcc_S1D1: break;
		case Tcc_S1D1S2D2: break;
		case Tcc_S2D2: break;
		case Trapcc: break;
		case Vsl: break;
		case ResolveCache: break;

		// Opcodes without parameters
		case Debug: 
		case DoForever:
		case DorForever:
		case Enddo:
		case Illegal:
		case Nop:
		case Pflush:
		case Pflushun:
		case Pfree:
		case Plockr:
		case Punlockr:
		case Reset:
		case Rti:
		case Rts:
		case Stop:
		case Trap:
		case Wait:
			return 1;
		case Parallel:
		case InstructionCount:
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
