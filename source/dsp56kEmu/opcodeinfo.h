#pragma once
#include <cstdint>

#include "opcodetypes.h"

namespace dsp56k
{
	constexpr static uint32_t createMask(const char (&_opcode)[25], const char _c, const char _c2)
	{
		constexpr auto len = 24;

		uint32_t mask = 0;

		for(size_t i=0; i<len; ++i)
		{
			if(_opcode[i] == _c || _opcode[i] == _c2)
			{
				const auto bit = 23 - i;

				mask |= (1<<bit);
			}
		}
		return mask;
	}

	struct OpcodeInfo
	{
		constexpr OpcodeInfo(const Instruction _inst, const char(&_opcode)[25], const char* _assembly, const ExtensionWordTypes _extensionWordType = None, const uint32_t _flags = 0)
			: m_instruction(_inst)
			, m_opcode(_opcode)
			, m_assembly(_assembly)
			, m_extensionWordType(_extensionWordType)
			, m_mask0(createMask(_opcode, '0', ~0))
			, m_mask1(createMask(_opcode, '1', ~0))
			, m_flags(_flags)
		{
		}

		const Instruction m_instruction;
		const char* const m_opcode;
		const char* const m_assembly;
		const ExtensionWordTypes m_extensionWordType;

		const uint32_t m_mask0;
		const uint32_t m_mask1;
		const uint32_t m_flags;

		Instruction getInstruction() const { return m_instruction; }

		static bool isParallelOpcode(const uint32_t _word)
		{
			return (_word >= 0x100000) || ((_word & 0xFE4000) == 0x080000);
		}

		bool flag(const OpcodeFlags _flag) const
		{
			return (m_flags & _flag) != 0;
		}

		bool flags(const OpcodeFlags _flagA, const OpcodeFlags _flagB) const
		{
			return flag(static_cast<OpcodeFlags>(_flagA | _flagB));
		}
	};

	/*
	 * Note: all 'o' bits are '0' in the docs, but according to the Motorola simulator disasm setting them to 1 still
	 * yields to valid instructions.
	 */
	constexpr OpcodeInfo g_opcodes[] =
	{
		OpcodeInfo(Abs,				"????????????????0010d110",	"ABS D"),
		OpcodeInfo(ADC,				"????????????????001Jd001",	"ADC S,D"),

		OpcodeInfo(Add_SD,			"????????????????0JJJd000",	"ADD S,D"),
		OpcodeInfo(Add_xx,			"0000000101iiiiii10ood000",	"ADD #xx,D"),
		OpcodeInfo(Add_xxxx,		"0000000101ooooo011ood000",	"ADD #xxxx,D", ImmediateData),
		OpcodeInfo(Addl,			"????????????????0001d010",	"ADDL S,D"),
		OpcodeInfo(Addr,			"????????????????0000d010",	"ADDR S,D"),

		OpcodeInfo(And_SD,			"????????????????01JJd110",	"AND S,D"),
		OpcodeInfo(And_xx,			"0000000101iiiiii10ood110",	"AND #xx,D"),
		OpcodeInfo(And_xxxx,		"0000000101ooooo011ood110",	"AND #xxxx,D", ImmediateData),
		OpcodeInfo(Andi,			"00000000iiiiiiii101110EE",	"AND(I) #xx,D"),

		OpcodeInfo(Asl_D,			"????????????????0011d010",	"ASL D"),
		OpcodeInfo(Asl_ii,			"0000110000011101SiiiiiiD",	"ASL #ii,S2,D"),
		OpcodeInfo(Asl_S1S2D,		"0000110000011110010SsssD",	"ASL S1,S2,D"),

		OpcodeInfo(Asr_D,			"????????????????0010d010",	"ASR D"),
		OpcodeInfo(Asr_ii,			"0000110000011100SiiiiiiD",	"ASR #ii,S2,D"),
		OpcodeInfo(Asr_S1S2D,		"0000110000011110011SsssD",	"ASR S1,S2,D"),

		OpcodeInfo(Bcc_xxxx,		"00001101000100000100CCCC",	"Bcc xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bcc_xxx,			"00000101CCCC01aaaa0aaaaa",	"Bcc xxx", None, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bcc_Rn,			"0000110100011RRR0100CCCC",	"Bcc Rn", None, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Bchg_ea,			"0000101101MMMRRR0S0bbbbb",	"BCHG #n,[X or Y]:ea", EffectiveAddress),	// Note: Doc typo, was ROS not R0S
		OpcodeInfo(Bchg_aa,			"0000101100aaaaaa0S0bbbbb",	"BCHG #n,[X or Y]:aa"),
		OpcodeInfo(Bchg_pp,			"0000101110pppppp0S0bbbbb",	"BCHG #n,[X or Y]:pp"),
		OpcodeInfo(Bchg_qq,			"0000000101qqqqqq0S0bbbbb",	"BCHG #n,[X or Y]:qq"),
		OpcodeInfo(Bchg_D,			"0000101111DDDDDD010bbbbb",	"BCHG #n,D"),

		OpcodeInfo(Bclr_ea,			"0000101001MMMRRR0S0bbbbb",	"BCLR #n,[X or Y]:ea", EffectiveAddress),
		OpcodeInfo(Bclr_aa,			"0000101000aaaaaa0S0bbbbb",	"BCLR #n,[X or Y]:aa"),
		OpcodeInfo(Bclr_pp,			"0000101010pppppp0S0bbbbb",	"BCLR #n,[X or Y]:pp"),
		OpcodeInfo(Bclr_qq,			"0000000100qqqqqq0S0bbbbb",	"BCLR #n,[X or Y]:qq"),
		OpcodeInfo(Bclr_D,			"0000101011DDDDDD010bbbbb",	"BCLR #n,D"),

		OpcodeInfo(Bra_xxxx,		"000011010001000011000000",	"BRA xxxx", PCRelativeAddressExt, OpFlagBranch),
		OpcodeInfo(Bra_xxx,			"00000101000011aaaa0aaaaa",	"BRA xxx", None, OpFlagBranch),
		OpcodeInfo(Bra_Rn,			"0000110100011RRR11000000",	"BRA Rn", None, OpFlagBranch),

		OpcodeInfo(Brclr_ea,		"0000110010MMMRRR0S0bbbbb",	"BRCLR #n,[X or Y]:ea,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brclr_aa,		"0000110010aaaaaa1S0bbbbb",	"BRCLR #n,[X or Y]:aa,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brclr_pp,		"0000110011pppppp0S0bbbbb",	"BRCLR #n,[X or Y]:pp,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brclr_qq,		"0000010010qqqqqq0S0bbbbb",	"BRCLR #n,[X or Y]:qq,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brclr_S,			"0000110011DDDDDD100bbbbb",	"BRCLR #n,S,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(BRKcc,			"00000000000000100001CCCC",	"BRKcc"),

		OpcodeInfo(Brset_ea,		"0000110010MMMRRR0S1bbbbb",	"BRSET #n,[X or Y]:ea,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brset_aa,		"0000110010aaaaaa1S1bbbbb",	"BRSET #n,[X or Y]:aa,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brset_pp,		"0000110011pppppp0S1bbbbb",	"BRSET #n,[X or Y]:pp,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brset_qq,		"0000010010qqqqqq0S1bbbbb",	"BRSET #n,[X or Y]:qq,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Brset_S,			"0000110011DDDDDD101bbbbb",	"BRSET #n,S,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(BScc_xxxx,		"00001101000100000000CCCC",	"BScc xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(BScc_xxx,		"00000101CCCC00aaaa0aaaaa",	"BScc xxx", None, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(BScc_Rn,			"0000110100011RRR0000CCCC",	"BScc Rn", None, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Bsclr_ea,		"0000110110MMMRRR0S0bbbbb",	"BSCLR #n,[X or Y]:ea,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsclr_aa,		"0000110110aaaaaa1S0bbbbb",	"BSCLR #n,[X or Y]:aa,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsclr_pp,		"0000110111pppppp0S0bbbbb",	"BSCLR #n,[X or Y]:pp,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsclr_qq,		"0000010010qqqqqq1S0bbbbb",	"BSCLR #n,[X or Y]:qq,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsclr_S,			"0000110111DDDDDD100bbbbb",	"BSCLR #n,S,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Bset_ea,			"0000101001MMMRRR0S1bbbbb",	"BSET #n,[X or Y]:ea", EffectiveAddress),
		OpcodeInfo(Bset_aa,			"0000101000aaaaaa0S1bbbbb",	"BSET #n,[X or Y]:aa"),
		OpcodeInfo(Bset_pp,			"0000101010pppppp0S1bbbbb",	"BSET #n,[X or Y]:pp"),
		OpcodeInfo(Bset_qq,			"0000000100qqqqqq0S1bbbbb",	"BSET #n,[X or Y]:qq"),
		OpcodeInfo(Bset_D,			"0000101011DDDDDD011bbbbb",	"BSET #n,D"),

		OpcodeInfo(Bsr_xxxx,		"000011010001000010000000",	"BSR xxxx", PCRelativeAddressExt, OpFlagBranch),
		OpcodeInfo(Bsr_xxx,			"00000101000010aaaa0aaaaa",	"BSR xxx", None, OpFlagBranch),
		OpcodeInfo(Bsr_Rn,			"0000110100011RRR10000000",	"BSR Rn", None, OpFlagBranch),

		OpcodeInfo(Bsset_ea,		"0000110110MMMRRR0S1bbbbb",	"BSSET #n,[X or Y]:ea,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsset_aa,		"0000110110aaaaaa1S1bbbbb",	"BSSET #n,[X or Y]:aa,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsset_pp,		"0000110111pppppp0S1bbbbb",	"BSSET #n,[X or Y]:pp,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsset_qq,		"0000010010qqqqqq1S1bbbbb",	"BSSET #n,[X or Y]:qq,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Bsset_S,			"0000110111DDDDDD101bbbbb",	"BSSET #n,S,xxxx", PCRelativeAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Btst_ea,			"0000101101MMMRRR0S1bbbbb",	"BTST #n,[X or Y]:ea", EffectiveAddress),		// Note: Doc typo, was ROS not R0S
		OpcodeInfo(Btst_aa,			"0000101100aaaaaa0S1bbbbb",	"BTST #n,[X or Y]:aa"),
		OpcodeInfo(Btst_pp,			"0000101110pppppp0S1bbbbb",	"BTST #n,[X or Y]:pp"),
		OpcodeInfo(Btst_qq,			"0000000101qqqqqq0S1bbbbb",	"BTST #n,[X or Y]:qq"),
		OpcodeInfo(Btst_D,			"0000101111DDDDDD011bbbbb",	"BTST #n,D"),

		OpcodeInfo(Clb,				"0000110000011110000000SD",	"CLB S,D"),
		OpcodeInfo(Clr,				"????????????????0001d011",	"CLR D"),

		OpcodeInfo(Cmp_S1S2,		"????????????????0JJJd101",	"CMP S1, S2"),
		OpcodeInfo(Cmp_xxS2,		"0000000101iiiiii10ood101",	"CMP #xx, S2"),
		OpcodeInfo(Cmp_xxxxS2,		"0000000101ooooo011ood101",	"CMP #xxxx,S2", ImmediateData),
		OpcodeInfo(Cmpm_S1S2,		"????????????????0JJJd111",	"CMPM S1, S2"),
		OpcodeInfo(Cmpu_S1S2,		"00001100000111111111gggd",	"CMPU S1, S2"),

		OpcodeInfo(Debug,			"000000000000001000000000",	"DEBUG"),
		OpcodeInfo(Debugcc,			"00000000000000110000CCCC",	"DEBUGcc"),

		OpcodeInfo(Dec,				"00000000000000000000101d",	"DEC D"),

		OpcodeInfo(Div,				"0000000110oooooo01JJdooo",	"DIV S,D"),

		OpcodeInfo(Dmac,			"000000010010010s1SdkQQQQ",	"DMAC (+/-)S1,S2,D"),

		OpcodeInfo(Do_ea,			"0000011001MMMRRR0S000000",	"DO [X or Y]:ea, expr", AbsoluteAddressExt, OpFlagLoop),
		OpcodeInfo(Do_aa,			"0000011000aaaaaa0S000000",	"DO [X or Y]:aa, expr", AbsoluteAddressExt, OpFlagLoop),
		OpcodeInfo(Do_xxx,			"00000110iiiiiiii1000hhhh",	"DO #xxx, expr", AbsoluteAddressExt, OpFlagLoop),
		OpcodeInfo(Do_S,			"0000011011DDDDDD00000000",	"DO S, expr", AbsoluteAddressExt, OpFlagLoop),
		OpcodeInfo(DoForever,		"000000000000001000000011",	"DO FOREVER", AbsoluteAddressExt, OpFlagLoop),

		OpcodeInfo(Dor_ea,			"0000011001MMMRRR0S010000",	"DOR [X or Y]:ea,label", PCRelativeAddressExt, OpFlagLoop),
		OpcodeInfo(Dor_aa,			"0000011000aaaaaa0S010000",	"DOR [X or Y]:aa,label", PCRelativeAddressExt, OpFlagLoop),
		OpcodeInfo(Dor_xxx,			"00000110iiiiiiii1001hhhh",	"DOR #xxx, label", PCRelativeAddressExt, OpFlagLoop),
		OpcodeInfo(Dor_S,			"0000011011DDDDDD00010000",	"DOR S, label", PCRelativeAddressExt, OpFlagLoop),
		OpcodeInfo(DorForever,		"000000000000001000000010",	"DOR FOREVER", PCRelativeAddressExt, OpFlagLoop),

		OpcodeInfo(Enddo,			"00000000000000001o0o1100",	"ENDDO"),

		OpcodeInfo(Eor_SD,			"????????????????01JJd011",	"EOR S,D"),
		OpcodeInfo(Eor_xx,			"0000000101iiiiii10ood011",	"EOR #xx,D"),
		OpcodeInfo(Eor_xxxx,		"0000000101ooooo011ood011",	"EOR #xxxx,D", ImmediateData),

		OpcodeInfo(Extract_S1S2,	"0000110000011010000sSSSD",	"EXTRACT S1,S2,D"),
		OpcodeInfo(Extract_CoS2,	"0000110000011000000s000D",	"EXTRACT #CO,S2,D", ImmediateData),
		OpcodeInfo(Extractu_S1S2,	"0000110000011010100sSSSD",	"EXTRACTU S1,S2,D"),
		OpcodeInfo(Extractu_CoS2,	"0000110000011000100s000D",	"EXTRACTU #CO,S2,D", ImmediateData),

		OpcodeInfo(Ifcc,			"001000000010CCCC????????",	"IFcc", None, OpFlagCondition),
		OpcodeInfo(Ifcc_U,			"001000000011CCCC????????",	"IFcc.U", None, OpFlagCondition),

		OpcodeInfo(Illegal,			"000000000000000000000101",	"ILLEGAL"),

		OpcodeInfo(Inc,				"00000000000000000000100d",	"INC D"),

		OpcodeInfo(Insert_S1S2,		"00001100000110110qqqSSSD",	"INSERT S1,S2,D"),
		OpcodeInfo(Insert_CoS2,		"00001100000110010qqq000D",	"INSERT #CO,S2,D", ImmediateData),

		OpcodeInfo(Jcc_xxx,			"00001110CCCCaaaaaaaaaaaa",	"Jcc xxx", None, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jcc_ea,			"0000101011MMMRRR1010CCCC",	"Jcc ea", EffectiveAddress, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Jclr_ea,			"0000101001MMMRRR1S0bbbbb",	"JCLR #n,[X or Y]:ea,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jclr_aa,			"0000101000aaaaaa1S0bbbbb",	"JCLR #n,[X or Y]:aa,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jclr_pp,			"0000101010pppppp1S0bbbbb",	"JCLR #n,[X or Y]:pp,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jclr_qq,			"0000000110qqqqqq1S0bbbbb",	"JCLR #n,[X or Y]:qq,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jclr_S,			"0000101011DDDDDD000bbbbb",	"JCLR #n,S,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),				// Documentation issue? used to be 0bbbb but we need bbbbb

		OpcodeInfo(Jmp_ea,			"0000101011MMMRRR10000000",	"JMP ea", EffectiveAddress, OpFlagBranch),
		OpcodeInfo(Jmp_xxx,			"000011000000aaaaaaaaaaaa",	"JMP xxx", None, OpFlagBranch),

		OpcodeInfo(Jscc_xxx,		"00001111CCCCaaaaaaaaaaaa",	"JScc xxx", None, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jscc_ea,			"0000101111MMMRRR1010CCCC",	"JScc ea", EffectiveAddress, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Jsclr_ea,		"0000101101MMMRRR1S0bbbbb",	"JSCLR #n,[X or Y]:ea,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsclr_aa,		"0000101100aaaaaa1S0bbbbb",	"JSCLR #n,[X or Y]:aa,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsclr_pp,		"0000101110pppppp1S0bbbbb",	"JSCLR #n,[X or Y]:pp,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsclr_qq,		"0000000111qqqqqq1S0bbbbb",	"JSCLR #n,[X or Y]:qq,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsclr_S,			"0000101111DDDDDD000bbbbb",	"JSCLR #n,S,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Jset_ea,			"0000101001MMMRRR1S1bbbbb",	"JSET #n,[X or Y]:ea,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jset_aa,			"0000101000aaaaaa1S1bbbbb",	"JSET #n,[X or Y]:aa,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jset_pp,			"0000101010pppppp1S1bbbbb",	"JSET #n,[X or Y]:pp,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jset_qq,			"0000000110qqqqqq1S1bbbbb",	"JSET #n,[X or Y]:qq,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jset_S,			"0000101011DDDDDD001bbbbb",	"JSET #n,S,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Jsr_ea,			"0000101111MMMRRR10000000",	"JSR ea", EffectiveAddress, OpFlagBranch),
		OpcodeInfo(Jsr_xxx,			"000011010000aaaaaaaaaaaa",	"JSR xxx", None, OpFlagBranch),

		OpcodeInfo(Jsset_ea,		"0000101101MMMRRR1S1bbbbb",	"JSSET #n,[X or Y]:ea,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsset_aa,		"0000101100aaaaaa1S1bbbbb",	"JSSET #n,[X or Y]:aa,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsset_pp,		"0000101110pppppp1S1bbbbb",	"JSSET #n,[X or Y]:pp,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsset_qq,		"0000000111qqqqqq1S1bbbbb",	"JSSET #n,[X or Y]:qq,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),
		OpcodeInfo(Jsset_S,			"0000101111DDDDDD001bbbbb",	"JSSET #n,S,xxxx", AbsoluteAddressExt, OpFlagBranch | OpFlagCondition),

		OpcodeInfo(Lra_Rn,			"0000010011000RRR000ddddd",	"LRA Rn,D"),
		OpcodeInfo(Lra_xxxx,		"0000010001oooooo010ddddd",	"LRA xxxx,D", PCRelativeAddressExt),

		OpcodeInfo(Lsl_D,			"????????????????0011D011",	"LSL D"),
		OpcodeInfo(Lsl_ii,			"000011000001111010iiiiiD",	"LSL #ii,D"),
		OpcodeInfo(Lsl_SD,			"00001100000111100001sssD",	"LSL S,D"),

		OpcodeInfo(Lsr_D,			"????????????????0010D011",	"LSR D"),
		OpcodeInfo(Lsr_ii,			"000011000001111011iiiiiD",	"LSR #ii,D"),
		OpcodeInfo(Lsr_SD,			"00001100000111100011sssD",	"LSR S,D"),

		OpcodeInfo(Lua_ea,			"00000100010MMRRR000ddddd",	"LUA/LEA ea,D"),
		OpcodeInfo(Lua_Rn,			"0000010000aaaRRRaaaadddd",	"LUA/LEA (Rn + aa),D"),

		OpcodeInfo(Mac_S1S2,		"????????????????1QQQdk10",	"MAC (+/-)S1,S2,D / MAC (+/-)S2,S1,D"),
		OpcodeInfo(Mac_S,			"00000001000sssss11QQdk10",	"MAC (+/-)S,#n,D"),				// Not: Doc fault, one s is missing
		OpcodeInfo(Maci_xxxx,		"0000000101ooooo111qqdk10",	"MACI (+/-)#xxxx,S,D", ImmediateData),
		OpcodeInfo(Macsu,			"00000001001001101sdkQQQQ",	"MACsu (+/-)S1,S2,D / MACuu (+/-)S1,S2,D"),
		OpcodeInfo(Macr_S1S2,		"????????????????1QQQdk11",	"MACR (+/-)S1,S2,D / MACR (+/-)S2,S1,D"),
		OpcodeInfo(Macr_S,			"00000001000sssss11QQdk11",	"MACR (+/-)S,#n,D"),			// Note: Doc says 0000000100003sss11QQdk11, but a 3? Seems to be an s and we need 5 s, same error as in MAC
		OpcodeInfo(Macri_xxxx,		"0000000101ooooo111qqdk11",	"MACRI (+/-)#xxxx,S,D", ImmediateData),

		OpcodeInfo(Max,				"????????????????00011101",	"MAX A, B"),
		OpcodeInfo(Maxm,			"????????????????00010101",	"MAXM A, B"),

		OpcodeInfo(Merge,			"00001100000110111000SSSD",	"MERGE S,D"),

		OpcodeInfo(Move_Nop,		"0010000000000000????????",	"MOVE S,D"),																										// NO Parallel Data Move
		OpcodeInfo(Move_xx,			"001dddddiiiiiiii????????",	"(...) #xx,D"),																										// Immediate Short Data Move
		OpcodeInfo(Mover,			"001000eeeeeddddd????????",	"(...) S,D"),																										// Register-to-Register Data Move
		OpcodeInfo(Move_ea,			"00100000010MMRRR????????",	"(...) ea"),																										// Address Register Update

		// X Memory Data Move
		OpcodeInfo(Movex_ea,		"01dd0dddW1MMMRRR????????",	"(...) X:ea,D / (...) S,X:ea / (...) #xxxxxx,D", EAandID),
		OpcodeInfo(Movex_aa,		"01dd0dddW0aaaaaa????????",	"(...) X:aa,D / (...) S,X:aa"),
		OpcodeInfo(Movex_Rnxxxx,	"0000101001110RRR1WDDDDDD",	"MOVE X:(Rn + xxxx),D / MOVE S,X:(Rn + xxxx)", PCRelativeAddressExt),									// ImData = Rn Relative Displacement
		OpcodeInfo(Movex_Rnxxx,		"0000001aaaaaaRRR1a0WDDDD",	"MOVE X:(Rn + xxx),D / MOVE S,X:(Rn + xxx)"),
		OpcodeInfo(Movexr_ea,		"0001ffdFW0MMMRRR????????",	"(...) X:ea,D1 S2,D2 / (...) S1,X:ea S2, D2 / (...) #xxxx,D1 S2,D2", EAandID),							// X Memory and Register Data Move
		OpcodeInfo(Movexr_A,		"0000100d00MMMRRR????????",	"(...) A -> X:ea X0 -> A / (...) B -> X:ea X0 -> B", EAandID),

		// Y Memory Data Move
		OpcodeInfo(Movey_ea,		"01dd1dddW1MMMRRR????????",	"(...) Y:ea,D / (...) S,Y:ea / (...) #xxxx,D", EAandID),
		OpcodeInfo(Movey_aa,		"01dd1dddW0aaaaaa????????",	"(...) Y:aa,D / (...) S,Y:aa"),
		OpcodeInfo(Movey_Rnxxxx,	"0000101101110RRR1WDDDDDD",	"MOVE Y:(Rn + xxxx),D / MOVE D,Y:(Rn + xxxx)", PCRelativeAddressExt),									// Rn Relative Displacement
		OpcodeInfo(Movey_Rnxxx,		"0000001aaaaaaRRR1a1WDDDD",	"MOVE Y:(Rn + xxx),D / MOVE D,Y:(Rn + xxx)"),
		OpcodeInfo(Moveyr_ea,		"0001deffW1MMMRRR????????",	"(...) S1,D1 Y:ea,D2 / (...) S1,D1 S2,Y:ea / (...) S1,D1 #xxxx,D2", EAandID),							// Register and Y Memory Data Move
		OpcodeInfo(Moveyr_A,		"0000100d10MMMRRR????????",	"(...) Y0 -> A A -> Y:ea / (...) Y0 -> B B -> Y:ea", EAandID),

		OpcodeInfo(Movel_ea,		"0100L0LLW1MMMRRR????????",	"(...) L:ea,D / (...) S,L:ea", EAandID),																// Long Memory Data Move
		OpcodeInfo(Movel_aa,		"0100L0LLW0aaaaaa????????",	"(...) L:aa,D / (...) S,L:aa"),

		OpcodeInfo(Movexy,			"1wmmeeffWrrMMRRR????????",	"(...) X:<eax>,D1 Y:<eay>,D2 / (...) X:<eax>,D1 S2,Y:<eay> / (...) S1,X:<eax> Y:<eay>,D2 / (...) S1,X:<eax> S2,Y:<eay>"),		//	XY Memory Data Move

		OpcodeInfo(Movec_ea,		"00000101W1MMMRRR0S1DDDDD",	"MOVE(C) [X or Y]:ea,D1 / MOVE(C) S1,[X or Y]:ea / MOVE(C) #xxxx,D1", EAandID),							// Move Control Register, Note: Doc typo, was ROS not R0S. Doc says its "effective address extension" but it is immediate data
		OpcodeInfo(Movec_aa,		"00000101W0aaaaaa0S1DDDDD",	"MOVE(C) [X or Y]:aa,D1 / MOVE(C) S1,[X or Y]:aa"),
		OpcodeInfo(Movec_S1D2,		"00000100W1eeeeee1o1DDDDD",	"MOVE(C) S1,D2 / MOVE(C) S2,D1"),
		OpcodeInfo(Movec_xx,		"00000101iiiiiiii101DDDDD",	"MOVE(C) #xx,D1"),

		OpcodeInfo(Movem_ea,		"00000111W1MMMRRR10dddddd",	"MOVE(M) S,P:ea / MOVE(M) P:ea,D", EffectiveAddress),													// Move Program Memory
		OpcodeInfo(Movem_aa,		"00000111W0aaaaaa00dddddd",	"MOVE(M) S,P:aa / MOVE(M) P:aa,D"),

		OpcodeInfo(Movep_ppea,		"0000100sW1MMMRRR1Spppppp",	"MOVEP [X or Y]:pp,[X or Y]:ea / MOVEP [X or Y]:ea,[X or Y]:pp", EAandID),
		OpcodeInfo(Movep_Xqqea,		"00000111W1MMMRRR0Sqqqqqq",	"MOVEP X:qq,[X or Y]:ea / MOVEP [X or Y]:ea,X:qq", EAandID),
		OpcodeInfo(Movep_Yqqea,		"00000111W0MMMRRR1Sqqqqqq",	"MOVEP Y:qq,[X or Y]:ea / MOVEP [X or Y]:ea,Y:qq", EAandID),
		OpcodeInfo(Movep_eapp,		"0000100sW1MMMRRR01pppppp",	"MOVEP P:ea,[X or Y]:pp / MOVEP [X or Y]:pp,P:ea", EAandID),		// another doc issue? These two opcodes are NOT mentioned with an optional extension word. But they have MMMRRR so this is needed
		OpcodeInfo(Movep_eaqq,		"000000001WMMMRRR0Sqqqqqq",	"MOVEP P:ea,[X or Y]:qq / MOVEP [X or Y]:qq,P:ea", EAandID),
		OpcodeInfo(Movep_Spp,		"0000100sW1dddddd00pppppp",	"MOVEP S,[X or Y]:pp / MOVEP [X or Y]:pp,D"),
		OpcodeInfo(Movep_SXqq,		"00000100W1dddddd1q0qqqqq",	"MOVEP S,X:qq / MOVEP X:qq,D"),
		OpcodeInfo(Movep_SYqq,		"00000100W1dddddd0q1qqqqq",	"MOVEP S,Y:qq / MOVEP Y:qq,D"),

		OpcodeInfo(Mpy_S1S2D,		"????????????????1QQQdk00",	"MPY (+/-)S1,S2,D / MPY (+/-)S2,S1,D"),
		OpcodeInfo(Mpy_SD,			"00000001000sssss11QQdk00",	"MPY (+/-)S,#n,D"),
		OpcodeInfo(Mpy_su,			"00000001001001111sdkQQQQ",	"MPY su (+/-)S1,S2,D / MPY uu (+/-)S1,S2,D"),

		OpcodeInfo(Mpyi,			"0000000101ooooo111qqdk00",	"MPYI (+/-)#xxxx,S,D", ImmediateData),

		OpcodeInfo(Mpyr_S1S2D,		"????????????????1QQQdk01",	"MPYR (+/-)S1,S2,D / MPYR (+/-)S2,S1,D"),
		OpcodeInfo(Mpyr_SD,			"00000001000sssss11QQdk01",	"MPYR (+/-)S,#n,D"),

		OpcodeInfo(Mpyri,			"0000000101ooooo111qqdk01",	"MPYRI (+/-)#xxxx,S,D", ImmediateData),

		OpcodeInfo(Neg,				"????????????????0011d110",	"NEG D"),
		OpcodeInfo(Nop,				"000000000000000000000000",	"NOP"),
		OpcodeInfo(Norm,			"0000000111011RRR0001d101",	"NORM Rn,D"),
		OpcodeInfo(Normf,			"00001100000111100010sssD",	"NORMF S,D"),

		OpcodeInfo(Not,				"????????????????0001d111",	"NOT D"),

		OpcodeInfo(Or_SD,			"????????????????01JJd010",	"OR S,D"),
		OpcodeInfo(Or_xx,			"0000000101iiiiii10ood010",	"OR #xx,D"),
		OpcodeInfo(Or_xxxx,			"0000000101ooooo011ood010",	"OR #xxxx,D", ImmediateData),
		OpcodeInfo(Ori,				"00000000iiiiiiii111110EE",	"OR(I) #xx,D"),

		OpcodeInfo(Pflush,			"000000000000000000000011",	"PFLUSH", None, OpFlagCacheMod),
		OpcodeInfo(Pflushun,		"000000000000000000000001",	"PFLUSHUN", None, OpFlagCacheMod),
		OpcodeInfo(Pfree,			"000000000000000000000010",	"PFREE", None, OpFlagCacheMod),

		OpcodeInfo(Plock,			"0000101111MMMRRR10000001",	"PLOCK ea", EAandID, OpFlagCacheMod),
		OpcodeInfo(Plockr,			"000000000000000000001111",	"PLOCKR xxxx", PCRelativeAddressExt, OpFlagCacheMod),
		OpcodeInfo(Punlock,			"0000101011MMMRRR10000001",	"PUNLOCK ea", EAandID, OpFlagCacheMod),
		OpcodeInfo(Punlockr,		"000000000000000000001110",	"PUNLOCKR xxxx", PCRelativeAddressExt, OpFlagCacheMod),

		OpcodeInfo(Rep_ea,			"0000011001MMMRRR0S100000",	"REP [X or Y]:ea", None, OpFlagLoop),
		OpcodeInfo(Rep_aa,			"0000011000aaaaaa0S100000",	"REP [X or Y]:aa", None, OpFlagLoop),
		OpcodeInfo(Rep_xxx,			"00000110iiiiiiii1o1ohhhh",	"REP #xxx", None, OpFlagLoop),
		OpcodeInfo(Rep_S,			"0000011011dddddd00100000",	"REP S", None, OpFlagLoop),

		OpcodeInfo(Reset,			"00000000000000001o0o0100",	"RESET"),

		OpcodeInfo(Rnd,				"????????????????0001d001",	"RND D"),
		OpcodeInfo(Rol,				"????????????????0011d111",	"ROL D"),
		OpcodeInfo(Ror,				"????????????????0010d111",	"ROR D"),
		OpcodeInfo(Rti,				"000000000000000000000100",	"RTI"),
		OpcodeInfo(Rts,				"000000000000000000001100",	"RTS"),

		OpcodeInfo(Sbc,				"????????????????001Jd101",	"SBC S,D"),

		OpcodeInfo(Stop,			"00000000000000001o0o0111",	"STOP"),

		OpcodeInfo(Sub_SD,			"????????????????0JJJd100",	"SUB S,D"),
		OpcodeInfo(Sub_xx,			"0000000101iiiiii10ood100",	"SUB #xx,D"),
		OpcodeInfo(Sub_xxxx,		"0000000101ooooo011ood100",	"SUB #xxxx,D", ImmediateData),

		OpcodeInfo(Subl,			"????????????????0001d110",	"SUBL S,D"),
		OpcodeInfo(subr,			"????????????????0000d110",	"SUBR S,D"),

		OpcodeInfo(Tcc_S1D1,		"00000010CCCC0ooo0JJJdooo",	"Tcc S1,D1", None, OpFlagCondition),
		OpcodeInfo(Tcc_S1D1S2D2,	"00000011CCCCottt0JJJdTTT",	"Tcc S1,D1 S2,D2", None, OpFlagCondition),
		OpcodeInfo(Tcc_S2D2,		"00000010CCCC1ttt0ooooTTT",	"Tcc S2,D2", None, OpFlagCondition),

		OpcodeInfo(Tfr,				"????????????????0JJJd001",	"TFR S,D"),
		OpcodeInfo(Trap,			"000000000000000000000110",	"TRAP"),
		OpcodeInfo(Trapcc,			"00000000000000000001CCCC",	"TRAPcc", None, OpFlagCondition),

		OpcodeInfo(Tst,				"????????????????0000d011",	"TST S"),

		OpcodeInfo(Vsl,				"0000101S11MMMRRR110i0000",	"VSL S,i,L:ea"),
		OpcodeInfo(Wait,			"00000000000000001o0o0110",	"WAIT"),

		// Dummy entry
		OpcodeInfo(ResolveCache,	"000000000000000000000000",	"ResolveCache"),
		OpcodeInfo(Parallel,		"000000000000000000000000",	"Parallel")
	};

	constexpr size_t g_opcodeCount = sizeof(g_opcodes) / sizeof(g_opcodes[0]);
}
