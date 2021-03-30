#include "pch.h"
#include "opcodes.h"

namespace dsp56k
{
	/*
	 * Note: all 'o' bits are '0' in the docs, but according to the Motorola simulator disasm setting them to 1 still
	 * yields to valid instructions.
	 */
	OpcodeInfo g_opcodes[] =
	{
		OpcodeInfo(OpcodeInfo::Abs,				"????????????????0010d110",	"ABS D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::ADC,				"????????????????001Jd001",	"ADC S,D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Add_SD,			"????????????????0JJJd000",	"ADD S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Add_xx,			"0000000101iiiiii10ood000",	"ADD #xx,D"),
		OpcodeInfo(OpcodeInfo::Add_xxxx,		"0000000101ooooo011ood000",	"ADD #xxxx,D", OpcodeInfo::ImmediateData),
		OpcodeInfo(OpcodeInfo::Addl,			"????????????????0001d010",	"ADDL S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Addr,			"????????????????0000d010",	"ADDR S,D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::And_SD,			"????????????????01JJd110",	"AND S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::And_xx,			"0000000101iiiiii10ood110",	"AND #xx,D"),
		OpcodeInfo(OpcodeInfo::And_xxxx,		"0000000101ooooo011ood110",	"AND #xxxx,D", OpcodeInfo::ImmediateData),
		OpcodeInfo(OpcodeInfo::Andi,			"00000000iiiiiiii101110EE",	"AND(I) #xx,D"),

		OpcodeInfo(OpcodeInfo::Asl_D,			"????????????????0011d010",	"ASL D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Asl_ii,			"0000110000011101SiiiiiiD",	"ASL #ii,S2,D"),
		OpcodeInfo(OpcodeInfo::Asl_S1S2D,		"0000110000011110010SsssD",	"ASL S1,S2,D"),

		OpcodeInfo(OpcodeInfo::Asr_D,			"????????????????0010d010",	"ASR D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Asr_ii,			"0000110000011100SiiiiiiD",	"ASR #ii,S2,D"),
		OpcodeInfo(OpcodeInfo::Asr_S1S2D,		"0000110000011110011SsssD",	"ASR S1,S2,D"),

//		OpcodeInfo(OpcodeInfo::Bcc_xxxx,		"00000101CCCC01aaaa0aaaaa",	"Bcc xxxx", OpcodeInfo::EffectiveAddress),	// TODO: documentation is a bit weird here, is the address now part of an extension word or part of aaaa...? Lots of typos in there
		OpcodeInfo(OpcodeInfo::Bcc_xxx,			"00000101CCCC01aaaa0aaaaa",	"Bcc xxx"),
		OpcodeInfo(OpcodeInfo::Bcc_Rn,			"0000110100011RRR0100CCCC",	"Bcc Rn"),

		OpcodeInfo(OpcodeInfo::Bchg_ea,			"0000101101MMMRRR0S0bbbbb",	"BCHG #n,[X or Y]:ea", OpcodeInfo::EffectiveAddress),	// Note: Doc typo, was ROS not R0S
		OpcodeInfo(OpcodeInfo::Bchg_aa,			"0000101100aaaaaa0S0bbbbb",	"BCHG #n,[X or Y]:aa"),
		OpcodeInfo(OpcodeInfo::Bchg_pp,			"0000101110pppppp0S0bbbbb",	"BCHG #n,[X or Y]:pp"),
		OpcodeInfo(OpcodeInfo::Bchg_qq,			"0000000101qqqqqq0S0bbbbb",	"BCHG #n,[X or Y]:qq"),
		OpcodeInfo(OpcodeInfo::Bchg_D,			"0000101111DDDDDD010bbbbb",	"BCHG #n,D"),

		OpcodeInfo(OpcodeInfo::Bclr_ea,			"0000101001MMMRRR0S0bbbbb",	"BCLR #n,[X or Y]:ea", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bclr_aa,			"0000101000aaaaaa0S0bbbbb",	"BCLR #n,[X or Y]:aa"),
		OpcodeInfo(OpcodeInfo::Bclr_pp,			"0000101010pppppp0S0bbbbb",	"BCLR #n,[X or Y]:pp"),
		OpcodeInfo(OpcodeInfo::Bclr_qq,			"0000000100qqqqqq0S0bbbbb",	"BCLR #n,[X or Y]:qq"),
		OpcodeInfo(OpcodeInfo::Bclr_D,			"0000101011DDDDDD010bbbbb",	"BCLR #n,D"),

		OpcodeInfo(OpcodeInfo::Bra_xxxx,		"000011010001000011000000",	"BRA xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bra_xxx,			"00000101000011aaaa0aaaaa",	"BRA xxx"),
		OpcodeInfo(OpcodeInfo::Bra_Rn,			"0000110100011RRR11000000",	"BRA Rn"),

		OpcodeInfo(OpcodeInfo::Brclr_ea,		"0000110010MMMRRR0S0bbbbb",	"BRCLR #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brclr_aa,		"0000110010aaaaaa1S0bbbbb",	"BRCLR #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brclr_pp,		"0000110011pppppp0S0bbbbb",	"BRCLR #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brclr_qq,		"0000010010qqqqqq0S0bbbbb",	"BRCLR #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brclr_S,			"0000110011DDDDDD100bbbbb",	"BRCLR #n,S,xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::BRKcc,			"00000000000000100001CCCC",	"BRKcc"),

		OpcodeInfo(OpcodeInfo::Brset_ea,		"0000110010MMMRRR0S1bbbbb",	"BRSET #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brset_aa,		"0000110010aaaaaa1S1bbbbb",	"BRSET #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brset_pp,		"0000110011pppppp0S1bbbbb",	"BRSET #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brset_qq,		"0000010010qqqqqq0S1bbbbb",	"BRSET #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Brset_S,			"0000110011DDDDDD101bbbbb",	"BRSET #n,S,xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::BScc_xxxx,		"00001101000100000000CCCC",	"BScc xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::BScc_xxx,		"00000101CCCC00aaaa0aaaaa",	"BScc xxx"),
		OpcodeInfo(OpcodeInfo::BScc_Rn,			"0000110100011RRR0000CCCC",	"BScc Rn"),

		OpcodeInfo(OpcodeInfo::Bsclr_ea,		"0000110110MMMRRR0S0bbbbb",	"BSCLR #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsclr_aa,		"0000110110aaaaaa1S0bbbbb",	"BSCLR #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsclr_qq,		"0000010010qqqqqq1S0bbbbb",	"BSCLR #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsclr_pp,		"0000110111pppppp0S0bbbbb",	"BSCLR #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsclr_S,			"0000110111DDDDDD100bbbbb",	"BSCLR #n,S,xxxx"),

		OpcodeInfo(OpcodeInfo::Bset_ea,			"0000101001MMMRRR0S1bbbbb",	"BSET #n,[X or Y]:ea", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bset_aa,			"0000101000aaaaaa0S1bbbbb",	"BSET #n,[X or Y]:aa"),
		OpcodeInfo(OpcodeInfo::Bset_pp,			"0000101010pppppp0S1bbbbb",	"BSET #n,[X or Y]:pp"),
		OpcodeInfo(OpcodeInfo::Bset_qq,			"0000000100qqqqqq0S1bbbbb",	"BSET #n,[X or Y]:qq"),
		OpcodeInfo(OpcodeInfo::Bset_D,			"0000101011DDDDDD011bbbbb",	"BSET #n,D"),

		OpcodeInfo(OpcodeInfo::Bsr_xxxx,		"000011010001000010000000",	"BSR xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsr_xxx,			"00000101000010aaaa0aaaaa",	"BSR xxx"),
		OpcodeInfo(OpcodeInfo::Bsr_Rn,			"0000110100011RRR10000000",	"BSR Rn"),

		OpcodeInfo(OpcodeInfo::Bsset_ea,		"0000110110MMMRRR0S1bbbbb",	"BSSET #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsset_aa,		"0000110110aaaaaa1S1bbbbb",	"BSSET #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsset_pp,		"0000110111pppppp0S1bbbbb",	"BSSET #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsset_qq,		"0000010010qqqqqq1S1bbbbb",	"BSSET #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Bsset_S,			"0000110111DDDDDD101bbbbb",	"BSSET #n,S,xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Btst_ea,			"0000101101MMMRRR0S1bbbbb",	"BTST #n,[X or Y]:ea", OpcodeInfo::EffectiveAddress),		// Note: Doc typo, was ROS not R0S
		OpcodeInfo(OpcodeInfo::Btst_aa,			"0000101100aaaaaa0S1bbbbb",	"BTST #n,[X or Y]:aa"),
		OpcodeInfo(OpcodeInfo::Btst_pp,			"0000101110pppppp0S1bbbbb",	"BTST #n,[X or Y]:pp"),
		OpcodeInfo(OpcodeInfo::Btst_qq,			"0000000101qqqqqq0S1bbbbb",	"BTST #n,[X or Y]:qq"),
		OpcodeInfo(OpcodeInfo::Btst_D,			"0000101111DDDDDD011bbbbb",	"BTST #n,D"),

		OpcodeInfo(OpcodeInfo::Clb,				"0000110000011110000000SD",	"CLB S,D"),
		OpcodeInfo(OpcodeInfo::Clr,				"????????????????0001d011",	"CLR D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Cmp_S1S2,		"????????????????0JJJd101",	"CMP S1, S2", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Cmp_xxS2,		"0000000101iiiiii10ood101",	"CMP #xx, S2"),
		OpcodeInfo(OpcodeInfo::Cmp_xxxxS2,		"0000000101ooooo011ood101",	"CMP #xxxx,S2", OpcodeInfo::ImmediateData),
		OpcodeInfo(OpcodeInfo::Cmpm_S1S2,		"????????????????0JJJd111",	"CMPM S1, S2", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Cmpu_S1S2,		"00001100000111111111gggd",	"CMPU S1, S2"),

		OpcodeInfo(OpcodeInfo::Debug,			"000000000000001000000000",	"DEBUG"),
		OpcodeInfo(OpcodeInfo::Debugcc,			"00000000000000110000CCCC",	"DEBUGcc"),

		OpcodeInfo(OpcodeInfo::Dec,				"00000000000000000000101d",	"DEC D"),

		OpcodeInfo(OpcodeInfo::Div,				"0000000110oooooo01JJdooo",	"DIV S,D"),

		OpcodeInfo(OpcodeInfo::Dmac,			"000000010010010s1SdkQQQQ",	"DMAC (+/-)S1,S2,D"),

		OpcodeInfo(OpcodeInfo::Do_ea,			"0000011001MMMRRR0S000000",	"DO [X or Y]:ea, expr", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Do_aa,			"0000011000aaaaaa0S000000",	"DO [X or Y]:aa, expr", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Do_xxx,			"00000110iiiiiiii1000hhhh",	"DO #xxx, expr", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Do_S,			"0000011011DDDDDD00000000",	"DO S, expr", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::DoForever,		"000000000000001000000011",	"DO FOREVER", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Dor_ea,			"0000011001MMMRRR0S010000",	"DOR [X or Y]:ea,label", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Dor_aa,			"0000011000aaaaaa0S010000",	"DOR [X or Y]:aa,label", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Dor_xxx,			"00000110iiiiiiii1001hhhh",	"DOR #xxx, label", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Dor_S,			"0000011011DDDDDD00010000",	"DOR S, label", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::DorForever,		"000000000000001000000010",	"DOR FOREVER", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Enddo,			"00000000000000001o0o1100",	"ENDDO"),

		OpcodeInfo(OpcodeInfo::Eor_SD,			"????????????????01JJd011",	"EOR S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Eor_xx,			"0000000101iiiiii10ood011",	"EOR #xx,D"),
		OpcodeInfo(OpcodeInfo::Eor_xxxx,		"0000000101ooooo011ood011",	"EOR #xxxx,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Extract_S1S2,	"0000110000011010000sSSSD",	"EXTRACT S1,S2,D"),
		OpcodeInfo(OpcodeInfo::Extract_CoS2,	"0000110000011000000s000D",	"EXTRACT #CO,S2,D", OpcodeInfo::ImmediateData),
		OpcodeInfo(OpcodeInfo::Extractu_S1S2,	"0000110000011010100sSSSD",	"EXTRACTU S1,S2,D"),
		OpcodeInfo(OpcodeInfo::Extractu_CoS2,	"0000110000011000100s000D",	"EXTRACTU #CO,S2,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Ifcc,			"001000000010CCCC????????",	"IFcc"),
		OpcodeInfo(OpcodeInfo::Ifcc_U,			"001000000011CCCC????????",	"IFcc.U"),

		OpcodeInfo(OpcodeInfo::Illegal,			"000000000000000000000101",	"ILLEGAL"),

		OpcodeInfo(OpcodeInfo::Inc,				"00000000000000000000100d",	"INC D"),

		OpcodeInfo(OpcodeInfo::Insert_S1S2,		"00001100000110110qqqSSSD",	"INSERT S1,S2,D"),
		OpcodeInfo(OpcodeInfo::Insert_CoS2,		"00001100000110010qqq000D",	"INSERT #CO,S2,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Jcc_xxx,			"00001110CCCCaaaaaaaaaaaa",	"Jcc xxx"),
		OpcodeInfo(OpcodeInfo::Jcc_ea,			"0000101011MMMRRR1010CCCC",	"Jcc ea", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Jclr_ea,			"0000101001MMMRRR1S0bbbbb",	"JCLR #n,[X or Y]:ea,xxxx"),
		OpcodeInfo(OpcodeInfo::Jclr_aa,			"0000101000aaaaaa1S0bbbbb",	"JCLR #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jclr_pp,			"0000101010pppppp1S0bbbbb",	"JCLR #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jclr_qq,			"0000000110qqqqqq1S0bbbbb",	"JCLR #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jclr_S,			"0000101011DDDDDD000bbbbb",	"JCLR #n,S,xxxx", OpcodeInfo::EffectiveAddress),				// Documentation issue? used to be 0bbbb but we need bbbbb

		OpcodeInfo(OpcodeInfo::Jmp_ea,			"0000101011MMMRRR10000000",	"JMP ea", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jmp_xxx,			"000011000000aaaaaaaaaaaa",	"JMP xxx"),

		OpcodeInfo(OpcodeInfo::Jscc_xxx,		"00001111CCCCaaaaaaaaaaaa",	"JScc xxx"),
		OpcodeInfo(OpcodeInfo::Jscc_ea,			"0000101111MMMRRR1010CCCC",	"JScc ea", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Jsclr_ea,		"0000101101MMMRRR1S0bbbbb",	"JSCLR #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsclr_aa,		"0000101100aaaaaa1S0bbbbb",	"JSCLR #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsclr_pp,		"0000101110pppppp1S0bbbbb",	"JSCLR #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsclr_qq,		"0000000111qqqqqq1S0bbbbb",	"JSCLR #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsclr_S,			"0000101111DDDDDD000bbbbb",	"JSCLR #n,S,xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Jset_ea,			"0000101001MMMRRR1S1bbbbb",	"JSET #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jset_aa,			"0000101000aaaaaa1S1bbbbb",	"JSET #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jset_pp,			"0000101010pppppp1S1bbbbb",	"JSET #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jset_qq,			"0000000110qqqqqq1S1bbbbb",	"JSET #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jset_S,			"0000101011DDDDDD001bbbbb",	"JSET #n,S,xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Jsr_ea,			"0000101111MMMRRR10000000",	"JSR ea", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsr_xxx,			"000011010000aaaaaaaaaaaa",	"JSR xxx"),

		OpcodeInfo(OpcodeInfo::Jsset_ea,		"0000101101MMMRRR1S1bbbbb",	"JSSET #n,[X or Y]:ea,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsset_aa,		"0000101100aaaaaa1S1bbbbb",	"JSSET #n,[X or Y]:aa,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsset_pp,		"0000101110pppppp1S1bbbbb",	"JSSET #n,[X or Y]:pp,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsset_qq,		"0000000111qqqqqq1S1bbbbb",	"JSSET #n,[X or Y]:qq,xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Jsset_S,			"0000101111DDDDDD001bbbbb",	"JSSET #n,S,xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Lra_Rn,			"0000010011000RRR000ddddd",	"LRA Rn,D"),
		OpcodeInfo(OpcodeInfo::Lra_xxxx,		"0000010001oooooo010ddddd",	"LRA xxxx,D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Lsl_D,			"????????????????0011D011",	"LSL D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Lsl_ii,			"000011000001111010iiiiiD",	"LSL #ii,D"),
		OpcodeInfo(OpcodeInfo::Lsl_SD,			"00001100000111100001sssD",	"LSL S,D"),

		OpcodeInfo(OpcodeInfo::Lsr_D,			"????????????????0010D011",	"LSR D"),
		OpcodeInfo(OpcodeInfo::Lsr_ii,			"000011000001111011iiiiiD",	"LSR #ii,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Lsr_SD,			"00001100000111100011sssD",	"LSR S,D"),

		OpcodeInfo(OpcodeInfo::Lua_ea,			"00000100010MMRRR000ddddd",	"LUA/LEA ea,D"),
		OpcodeInfo(OpcodeInfo::Lua_Rn,			"0000010000aaaRRRaaaadddd",	"LUA/LEA (Rn + aa),D"),

		OpcodeInfo(OpcodeInfo::Mac_S1S2,		"????????????????1QQQdk10",	"MAC (+/-)S1,S2,D / MAC (+/-)S2,S1,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Mac_S,			"00000001000sssss11QQdk10",	"MAC (+/-)S,#n,D"),				// Not: Doc fault, one s is missing
		OpcodeInfo(OpcodeInfo::Maci_xxxx,		"0000000101ooooo111qqdk10",	"MACI (+/-)#xxxx,S,D", OpcodeInfo::ImmediateData),
		OpcodeInfo(OpcodeInfo::Macsu,			"00000001001001101sdkQQQQ",	"MACsu (+/-)S1,S2,D / MACuu (+/-)S1,S2,D"),
		OpcodeInfo(OpcodeInfo::Macr_S1S2,		"????????????????1QQQdk11",	"MACR (+/-)S1,S2,D / MACR (+/-)S2,S1,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Macr_S,			"00000001000sssss11QQdk11",	"MACR (+/-)S,#n,D"),			// Note: Doc says 0000000100003sss11QQdk11, but a 3? Seems to be an s and we need 5 s, same error as in MAC
		OpcodeInfo(OpcodeInfo::Macri_xxxx,		"0000000101ooooo111qqdk11",	"MACRI (+/-)#xxxx,S,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Max,				"????????????????00011101",	"MAX A, B", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Maxm,			"????????????????00010101",	"MAXM A, B", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Merge,			"00001100000110111000SSSD",	"MERGE S,D"),

		OpcodeInfo(OpcodeInfo::Move_Nop,		"0010000000000000????????",	"MOVE S,D"),																										// NO Parallel Data Move
		OpcodeInfo(OpcodeInfo::Move_xx,			"001dddddiiiiiiii????????",	"(...) #xx,D"),																										// Immediate Short Data Move
		OpcodeInfo(OpcodeInfo::Mover,			"001000eeeeeddddd????????",	"(...) S,D"),																										// Register-to-Register Data Move
		OpcodeInfo(OpcodeInfo::Move_ea,			"00100000010MMRRR????????",	"(...) ea"),																										// Address Register Update

		// X Memory Data Move
		OpcodeInfo(OpcodeInfo::Movex_ea,		"01dd0dddW1MMMRRR????????",	"(...) X:ea,D / (...) S,X:ea / (...) #xxxxxx,D", OpcodeInfo::EAandID),
		OpcodeInfo(OpcodeInfo::Movex_aa,		"01dd0dddW0aaaaaa????????",	"(...) X:aa,D / (...) S,X:aa"),
		OpcodeInfo(OpcodeInfo::Movex_Rnxxxx,	"0000101001110RRR1WDDDDDD",	"MOVE X:(Rn + xxxx),D / MOVE S,X:(Rn + xxxx)", OpcodeInfo::ImmediateData),											// ImData = Rn Relative Displacement
		OpcodeInfo(OpcodeInfo::Movex_Rnxxx,		"0000001aaaaaaRRR1a0WDDDD",	"MOVE X:(Rn + xxx),D / MOVE S,X:(Rn + xxx)"),
		OpcodeInfo(OpcodeInfo::Movexr_ea,		"0001ffdFW0MMMRRR????????",	"(...) X:ea,D1 S2,D2 / (...) S1,X:ea S2, D2 / (...) #xxxx,D1 S2,D2", OpcodeInfo::EAandID),							// X Memory and Register Data Move
		OpcodeInfo(OpcodeInfo::Movexr_A,		"0000100d00MMMRRR????????",	"(...) A -> X:ea X0 -> A / (...) B -> X:ea X0 -> B", OpcodeInfo::EffectiveAddress),

		// Y Memory Data Move
		OpcodeInfo(OpcodeInfo::Movey_ea,		"01dd1dddW1MMMRRR????????",	"(...) Y:ea,D / (...) S,Y:ea / (...) #xxxx,D", OpcodeInfo::EAandID),
		OpcodeInfo(OpcodeInfo::Movey_aa,		"01dd1dddW0aaaaaa????????",	"(...) Y:aa,D / (...) S,Y:aa"),
		OpcodeInfo(OpcodeInfo::Movey_Rnxxxx,	"0000101101110RRR1WDDDDDD",	"MOVE Y:(Rn + xxxx),D / MOVE D,Y:(Rn + xxxx)", OpcodeInfo::ImmediateData),											// ImData = Rn Relative Displacement
		OpcodeInfo(OpcodeInfo::Movey_Rnxxx,		"0000001aaaaaaRRR1a1WDDDD",	"MOVE Y:(Rn + xxx),D / MOVE D,Y:(Rn + xxx)"),
		OpcodeInfo(OpcodeInfo::Moveyr_ea,		"0001deffW1MMMRRR????????",	"(...) S1,D1 Y:ea,D2 / (...) S1,D1 S2,Y:ea / (...) S1,D1 #xxxx,D2", OpcodeInfo::EAandID),							// Register and Y Memory Data Move
		OpcodeInfo(OpcodeInfo::Moveyr_A,		"0000100d10MMMRRR????????",	"(...) Y0 -> A A -> Y:ea / (...) Y0 -> B B -> Y:ea", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Movel_ea,		"0100L0LLW1MMMRRR????????",	"(...) L:ea,D / (...) S,L:ea", OpcodeInfo::EffectiveAddress),														// Long Memory Data Move
		OpcodeInfo(OpcodeInfo::Movel_aa,		"0100L0LLW0aaaaaa????????",	"(...) L:aa,D / (...) S,L:aa"),

		OpcodeInfo(OpcodeInfo::Movexy,			"1wmmeeffWrrMMRRR????????",	"(...) X:<eax>,D1 Y:<eay>,D2 / (...) X:<eax>,D1 S2,Y:<eay> / (...) S1,X:<eax> Y:<eay>,D2 / (...) S1,X:<eax> S2,Y:<eay>"),		//	XY Memory Data Move

		OpcodeInfo(OpcodeInfo::Movec_ea,		"00000101W1MMMRRR0S1DDDDD",	"MOVE(C) [X or Y]:ea,D1 / MOVE(C) S1,[X or Y]:ea / MOVE(C) #xxxx,D1", OpcodeInfo::ImmediateData),					// Move Control Register, Note: Doc typo, was ROS not R0S. Doc says its "effective address extension" but it is immediate data
		OpcodeInfo(OpcodeInfo::Movec_aa,		"00000101W0aaaaaa0S1DDDDD",	"MOVE(C) [X or Y]:aa,D1 / MOVE(C) S1,[X or Y]:aa"),
		OpcodeInfo(OpcodeInfo::Movec_S1D2,		"00000100W1eeeeee1o1DDDDD",	"MOVE(C) S1,D2 / MOVE(C) S2,D1"),
		OpcodeInfo(OpcodeInfo::Movec_xx,		"00000101iiiiiiii101DDDDD",	"MOVE(C) #xx,D1"),

		OpcodeInfo(OpcodeInfo::Movem_ea,		"00000111W1MMMRRR10dddddd",	"MOVE(M) S,P:ea / MOVE(M) P:ea,D", OpcodeInfo::EffectiveAddress),													// Move Program Memory
		OpcodeInfo(OpcodeInfo::Movem_aa,		"00000111W0aaaaaa00dddddd",	"MOVE(M) S,P:aa / MOVE(M) P:aa,D"),

		OpcodeInfo(OpcodeInfo::Movep_ppea,		"0000100sW1MMMRRR1Spppppp",	"MOVEP [X or Y]:pp,[X or Y]:ea / MOVEP [X or Y]:ea,[X or Y]:pp", OpcodeInfo::EAandID),
		OpcodeInfo(OpcodeInfo::Movep_Xqqea,		"00000111W1MMMRRR0Sqqqqqq",	"MOVEP X:qq,[X or Y]:ea / MOVEP [X or Y]:ea,X:qq", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Movep_Yqqea,		"00000111W0MMMRRR1Sqqqqqq",	"MOVEP Y:qq,[X or Y]:ea / MOVEP [X or Y]:ea,Y:qq", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Movep_eapp,		"0000100sW1MMMRRR01pppppp",	"MOVEP P:ea,[X or Y]:pp / MOVEP [X or Y]:pp,P:ea", OpcodeInfo::EffectiveAddress),		// another doc issue? These two opcodes are NOT mentioned with an optional extension word. But they have MMMRRR so this is needed
		OpcodeInfo(OpcodeInfo::Movep_eaqq,		"000000001WMMMRRR0Sqqqqqq",	"MOVEP P:ea,[X or Y]:qq / MOVEP [X or Y]:qq,P:ea", static_cast<OpcodeInfo::ExtensionWordTypes>(OpcodeInfo::EffectiveAddress | OpcodeInfo::ImmediateData)),
		OpcodeInfo(OpcodeInfo::Movep_Spp,		"0000100sW1dddddd00pppppp",	"MOVEP S,[X or Y]:pp / MOVEP [X or Y]:pp,D"),
		OpcodeInfo(OpcodeInfo::Movep_SXqq,		"00000100W1dddddd1q0qqqqq",	"MOVEP S,X:qq / MOVEP X:qq,D"),
		OpcodeInfo(OpcodeInfo::Movep_SYqq,		"00000100W1dddddd0q1qqqqq",	"MOVEP S,Y:qq / MOVEP Y:qq,D"),

		OpcodeInfo(OpcodeInfo::Mpy_S1S2D,		"????????????????1QQQdk00",	"MPY (+/-)S1,S2,D / MPY (+/-)S2,S1,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Mpy_SD,			"00000001000sssss11QQdk00",	"MPY (+/-)S,#n,D"),
		OpcodeInfo(OpcodeInfo::Mpy_su,			"00000001001001111sdkQQQQ",	"MPY su (+/-)S1,S2,D / MPY uu (+/-)S1,S2,D"),

		OpcodeInfo(OpcodeInfo::Mpyi,			"0000000101ooooo111qqdk00",	"MPYI (+/-)#xxxx,S,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Mpyr_S1S2D,		"????????????????1QQQdk01",	"MPYR (+/-)S1,S2,D / MPYR (+/-)S2,S1,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Mpyr_SD,			"00000001000sssss11QQdk01",	"MPYR (+/-)S,#n,D"),

		OpcodeInfo(OpcodeInfo::Mpyri,			"0000000101ooooo111qqdk01",	"MPYRI (+/-)#xxxx,S,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Neg,				"????????????????0011d110",	"NEG D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Nop,				"000000000000000000000000",	"NOP"),
		OpcodeInfo(OpcodeInfo::Norm,			"0000000111011RRR0001d101",	"NORM Rn,D"),
		OpcodeInfo(OpcodeInfo::Normf,			"00001100000111100010sssD",	"NORMF S,D"),

		OpcodeInfo(OpcodeInfo::Not,				"????????????????0001d111",	"NOT D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Or_SD,			"????????????????01JJd010",	"OR S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Or_xx,			"0000000101iiiiii10ood010",	"OR #xx,D"),
		OpcodeInfo(OpcodeInfo::Or_xxxx,			"0000000101ooooo011ood010",	"OR #xxxx,D", OpcodeInfo::ImmediateData),
		OpcodeInfo(OpcodeInfo::Ori,				"00000000iiiiiiii111110EE",	"OR(I) #xx,D"),

		OpcodeInfo(OpcodeInfo::Pflush,			"000000000000000000000011",	"PFLUSH"),
		OpcodeInfo(OpcodeInfo::Pflushun,		"000000000000000000000001",	"PFLUSHUN"),
		OpcodeInfo(OpcodeInfo::Pfree,			"000000000000000000000010",	"PFREE"),

		OpcodeInfo(OpcodeInfo::Plock,			"0000101111MMMRRR10000001",	"PLOCK ea", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Plockr,			"000000000000000000001111",	"PLOCKR xxxx", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Punlock,			"0000101011MMMRRR10000001",	"PUNLOCK ea", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Punlockr,		"000000000000000000001110",	"PUNLOCKR xxxx", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Rep_ea,			"0000011001MMMRRR0S100000",	"REP [X or Y]:ea"),
		OpcodeInfo(OpcodeInfo::Rep_aa,			"0000011000aaaaaa0S100000",	"REP [X or Y]:aa"),
		OpcodeInfo(OpcodeInfo::Rep_xxx,			"00000110iiiiiiii1010hhhh",	"REP #xxx"),
		OpcodeInfo(OpcodeInfo::Rep_S,			"0000011011dddddd00100000",	"REP S"),

		OpcodeInfo(OpcodeInfo::Reset,			"00000000000000001o0o0100",	"RESET"),

		OpcodeInfo(OpcodeInfo::Rnd,				"????????????????0001d001",	"RND D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Rol,				"????????????????0011d111",	"ROL D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Ror,				"????????????????0010d111",	"ROR D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Rti,				"000000000000000000000100",	"RTI"),
		OpcodeInfo(OpcodeInfo::Rts,				"000000000000000000001100",	"RTS"),

		OpcodeInfo(OpcodeInfo::Sbc,				"????????????????001Jd101",	"SBC S,D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Stop,			"00000000000000001o0o0111",	"STOP"),

		OpcodeInfo(OpcodeInfo::Sub_SD,			"????????????????0JJJd100",	"SUB S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Sub_xx,			"0000000101iiiiii10ood100",	"SUB #xx,D"),
		OpcodeInfo(OpcodeInfo::Sub_xxxx,		"0000000101ooooo011ood100",	"SUB #xxxx,D", OpcodeInfo::ImmediateData),

		OpcodeInfo(OpcodeInfo::Subl,			"????????????????0001d110",	"SUBL S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::subr,			"????????????????0000d110",	"SUBR S,D", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Tcc_S1D1,		"00000010CCCC0ooo0JJJdooo",	"Tcc S1,D1"),
		OpcodeInfo(OpcodeInfo::Tcc_S1D1S2D2,	"00000011CCCCottt0JJJdTTT",	"Tcc S1,D1 S2,D2"),
		OpcodeInfo(OpcodeInfo::Tcc_S2D2,		"00000010CCCC1ttt0ooooTTT",	"Tcc S2,D2"),

		OpcodeInfo(OpcodeInfo::Tfr,				"????????????????0JJJd001",	"TFR S,D", OpcodeInfo::EffectiveAddress),
		OpcodeInfo(OpcodeInfo::Trap,			"000000000000000000000110",	"TRAP"),
		OpcodeInfo(OpcodeInfo::Trapcc,			"00000000000000000001CCCC",	"TRAPcc"),

		OpcodeInfo(OpcodeInfo::Tst,				"????????????????0000d011",	"TST S", OpcodeInfo::EffectiveAddress),

		OpcodeInfo(OpcodeInfo::Vsl,				"0000101S11MMMRRR110i0000",	"VSL S,i,L:ea"),
		OpcodeInfo(OpcodeInfo::Wait,			"00000000000000001o0o0110",	"WAIT")
	};

	OpcodeInfo::OpcodeInfo(Instruction _inst, const char* _opcode, const char* _assembly, ExtensionWordTypes _extensionWordType): m_instruction(_inst), m_opcode(_opcode), m_assembly(_assembly), m_extensionWordType(_extensionWordType)
	{
		const auto len = strlen(_opcode);

		if(_opcode[0] == ' ' || _opcode[0] == '\t')
			return;

		for(size_t i=0; i<len;)
		{
			const auto bit = 23 - i;

			const auto c = _opcode[i];

			if(c == '0' || c == 'O')	// TODO: I assume that the O is a typo in the docs and should be a 0. It is not mentioned in the instruction partial encoding, also missing in the instruction fields for the instructions that have it
			{
				m_mask0 |= (1<<bit);
				++i;
			}
			else if(c == '1')
			{
				m_mask1 |= (1<<bit);
				++i;
			}
			else
			{
				auto count = 1;

				auto j=i+1;

				for(; j<len; ++j)
				{
					if(_opcode[j] != c)
						break;
					++count;
				}

				i += count;

				auto createField = [&](char expectedChar, int expectedCount, Field field) -> bool
				{
					if(c != expectedChar || count != expectedCount)
						return false;

					m_fieldInfo[field] = FieldInfo(bit - count + 1, count);
					return true;
				};

				if(	createField('a', 1, Field_a) ||	
					createField('a', 3, Field_aaa) ||	
					createField('a', 4, Field_aaaa) ||	
					createField('a', 5, Field_aaaaa) ||	
					createField('a', 6, Field_aaaaaa) ||	
					createField('a', 12, Field_aaaaaaaaaaaa) ||	
					createField('b', 5, Field_bbbbb) ||	
					createField('C', 4, Field_CCCC) ||	
					createField('d', 1, Field_d) ||	
					createField('d', 2, Field_dd) ||
					createField('d', 3, Field_ddd) ||
					createField('d', 4, Field_dddd) ||
					createField('d', 5, Field_ddddd) ||
					createField('d', 6, Field_dddddd) ||
					createField('D', 1, Field_D) ||	
					createField('D', 4, Field_DDDD) ||
					createField('D', 5, Field_DDDDD) ||	
					createField('D', 6, Field_DDDDDD) ||	
					createField('e', 1, Field_e) ||		
					createField('e', 2, Field_ee) ||		
					createField('E', 2, Field_EE) ||		
					createField('f', 2, Field_ff) ||		
					createField('F', 1, Field_F) ||		
					createField('e', 5, Field_eeeee) ||		
					createField('e', 6, Field_eeeeee) ||		
					createField('g', 3, Field_ggg) ||		
					createField('h', 4, Field_hhhh) ||		
					createField('i', 1, Field_i) ||		
					createField('i', 5, Field_iiiii) ||		
					createField('i', 6, Field_iiiiii) ||		
					createField('i', 8, Field_iiiiiiii) ||
					createField('J', 1, Field_J) ||
					createField('J', 2, Field_JJ) ||		
					createField('J', 3, Field_JJJ) ||		
					createField('k', 1, Field_k) ||		
					createField('L', 1, Field_L) ||		
					createField('L', 2, Field_LL) ||		
					createField('m', 2, Field_mm) ||		
					createField('M', 2, Field_MM) ||		
					createField('M', 3, Field_MMM) ||		
					createField('p', 6, Field_pppppp) ||		
					createField('q', 2, Field_qq) ||		
					createField('q', 3, Field_qqq) ||
					createField('q', 1, Field_q) ||
					createField('q', 5, Field_qqqqq) ||
					createField('q', 6, Field_qqqqqq) ||
					createField('Q', 2, Field_QQ) ||		
					createField('Q', 3, Field_QQQ) ||		
					createField('Q', 4, Field_QQQQ) ||		
					createField('r', 2, Field_rr) ||		
					createField('R', 3, Field_RRR) ||		
					createField('s', 1, Field_s) ||
					createField('s', 3, Field_sss) ||		
					createField('s', 4, Field_ssss) ||		
					createField('s', 5, Field_sssss) ||		
					createField('S', 1, Field_S) ||
					createField('S', 3, Field_SSS) ||		
					createField('t', 3, Field_ttt) ||		
					createField('T', 3, Field_TTT) ||		
					createField('w', 1, Field_w) ||
					createField('W', 1, Field_W) ||
					createField('l', 1, Field_l) ||
					createField('o', 1, Field_o) ||
					createField('o', 2, Field_oo) ||
					createField('o', 3, Field_ooo) ||
					createField('o', 4, Field_oooo) ||
					createField('o', 5, Field_ooooo) ||
					createField('o', 6, Field_oooooo) ||
					createField('?', 8, Field_AluOperation) ||
					createField('?', 16, Field_MoveOperation) )
				{
					
				}
				else
				{
					assert(0 && "unknown field info");					
				}
			}
		}
	}

	Opcodes::Opcodes()
	{
		const auto len = sizeof(g_opcodes) / sizeof(g_opcodes[0]);

		m_opcodesAlu.reserve(len);
		m_opcodesMove.reserve(len);
		m_opcodesNonParallel.reserve(len);

		for(size_t i=0; i<len; ++i)
		{
			const OpcodeInfo& opcode = g_opcodes[i];

			if(opcode.isNonParallelOpcode())
				m_opcodesNonParallel.push_back(&opcode);
			else if(opcode.hasField(OpcodeInfo::Field_AluOperation))
				m_opcodesMove.push_back(&opcode);
			else if(opcode.hasField(OpcodeInfo::Field_MoveOperation))
				m_opcodesAlu.push_back(&opcode);
		}
	}

	const OpcodeInfo* Opcodes::findNonParallelOpcodeInfo(TWord _opcode) const
	{
		assert(isNonParallelOpcode(_opcode));
		return findOpcodeInfo(_opcode, m_opcodesNonParallel);
	}

	const OpcodeInfo* Opcodes::findParallelMoveOpcodeInfo(TWord _opcode) const
	{
		assert(isParallelOpcode(_opcode));
		return findOpcodeInfo(_opcode, m_opcodesMove);
	}

	const OpcodeInfo* Opcodes::findParallelAluOpcodeInfo(TWord _opcode) const
	{
		assert(isParallelOpcode(_opcode));
		return findOpcodeInfo(_opcode, m_opcodesAlu);
	}

	const OpcodeInfo* Opcodes::findOpcodeInfo(TWord _opcode, const std::vector<const OpcodeInfo*>& _opcodes)
	{
		const OpcodeInfo* res = nullptr;
		size_t resIndex = 0;

		for (size_t i=0; i<_opcodes.size(); ++i)
		{
			const auto* oi = _opcodes[i];

			if(!oi->m_mask1)
				continue;

			if(oi->match(_opcode))
			{
				if(res != nullptr)
					res->match(_opcode);
				assert(res == nullptr)
				res = oi;
				resIndex = i;
			}
		}
		return res;
	}
}
