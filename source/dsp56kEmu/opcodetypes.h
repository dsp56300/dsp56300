#pragma once

namespace dsp56k
{
	enum Field
	{
		Field_a,			// 7-bit Sign Extended Short Displacement Address
		Field_aaa,			// 7-bit Sign Extended Short Displacement Address
		Field_aaaa,			// Signed PC-Relative Short Displacement
		Field_aaaaa,		// Signed PC-Relative Short Displacement LSB, appears as aaaa0aaaaa
		Field_aaaaaa,		// Absolute Address [0–63]
		Field_aaaaaaaaaaaa,	// Short Jump Address
		Field_bbbbb,		// Bit number
		Field_CCCC,			// Condition Code
		Field_d,			// Destination accumulator [A/B]
		Field_dd,			// Five-Bit Register Encoding 1 MSB
		Field_ddd,			// Five-Bit Register Encoding 1 LSB
		Field_dddd,			// AGU Address and Offset Registers Encoding
		Field_ddddd,		// Five-Bit Register Encoding 1
		Field_dddddd,		// Source/Destination register [all on-chip registers]
		Field_D,			// Destination accumulator [A/B]
		Field_DDDD,			// ALU Registers Encoding
		Field_DDDDD,		// Program Controller Register (Is also lowercase ddddd in docs, but clashes with Five-Bit register encoding 1)
		Field_DDDDDD,		// Six-Bit Encoding for All On-Chip Registers
		Field_e,			// D1 input register [X0,X1]
		Field_ee,			// S1/D1 register [X0,X1,A,B]
		Field_EE,			// Program Control Unit Register Encoding
		Field_eeeee,		// Source Register
		Field_eeeeee,		// S2/D2 register [all on-chip registers]
		Field_ff,			// S2/D2 register [Y0,Y1,A,B]
		Field_F,			// D2 input register [Y0,Y1]
		Field_ggg,			// Data ALU Operands Encoding 3
		Field_hhhh,			// Immediate Short Data, 4 MSB bits to form a 12 bit value as hhhhiiiiiiii [0-4095]
		Field_i,			// Bit value, 0 or 1 to be placed in the least significant bit of Y:<ea>
		Field_iiiii,		// 5-bit unsigned integer [0–16] denoting the shift amount
		Field_iiiiii,		// 6-bit Immediate Short Data
		Field_iiiiiiii,		// 8-bit Immediate Short Data
		Field_J,			// Data ALU Source Operands Encoding
		Field_JJ,			// Data ALU Source Operands Encoding
		Field_JJJ,			// Data ALU Source Operands Encoding 2
		Field_k,			// Data ALU Multiply Sign Encoding
		Field_L,			// Two Data ALU registers
		Field_LL,			// Two Data ALU registers
		Field_mm,			// mmrr 4-bit Y Effective Address (R4–R7 or R0–R3)
		Field_MM,			// Effective Addressing Mode Encoding 4
		Field_MMM,			// Effective Addressing Mode Encoding 2/3
		Field_pppppp,		// I/O Short Address [64 addresses: $FFFFC0–$FFFFFF]
		Field_qq,			// Data ALU Multiply Operands Encoding 3
		Field_qqq,			// Data ALU Operands Encoding 3
		Field_q,			// I/O Short Address [64 addresses: $FFFF80–$FFFFBF]
		Field_qqqqq,		// I/O Short Address [64 addresses: $FFFF80–$FFFFBF]
		Field_qqqqqq,		// I/O Short Address [64 addresses: $FFFF80–$FFFFBF]
		Field_QQ,			// Data ALU Multiply Operands Encoding 2
		Field_QQQ,			// Data ALU Multiply Operands Encoding 1
		Field_QQQQ,			// Data ALU Multiply Operands Encoding 4
		Field_rr,			// mmrr 4-bit Y Effective Address (R4–R7 or R0–R3)
		Field_RRR,			// Effective Addressing Mode Encoding 2/3/4
		Field_s,			// Signed/Unsigned Partial Encoding 2 (su = 0, uu = 1)
		Field_sss,			// Control register [X0,X1,Y0,Y1,A1,B1]
		Field_ssss,			// Immediate Data ALU Operand Encoding
		Field_sssss,		// Immediate operand
		Field_S,			// Space (X or Y memory)
		Field_SSS,			// Data ALU Operands Encoding 3
		Field_ttt,			// Source address register [R0–R7]
		Field_TTT,			// Destination Address register [R0–R7]
		Field_w,			// Y move Operation Control
		Field_W,			// Write Control Encoding

		Field_l,			// Replacement for a 1 that is in the docs although the value can be either 0 or 1
		Field_o,			// Replacement for a 0 that is in the docs although the value can be either 0 or 1
		Field_oo,			// Replacement for a 0 that is in the docs although the value can be either 0 or 1
		Field_ooo,			// Replacement for a 0 that is in the docs although the value can be either 0 or 1
		Field_oooo,			// Replacement for a 0 that is in the docs although the value can be either 0 or 1
		Field_ooooo,		// Replacement for a 0 that is in the docs although the value can be either 0 or 1
		Field_oooooo,		// Replacement for a 0 that is in the docs although the value can be either 0 or 1

		Field_AluOperation,
		Field_MoveOperation,

		Field_COUNT
	};

	enum Instruction
	{
		Abs,
		ADC,
		Add_SD,		Add_xx,		Add_xxxx,
		Addl,		Addr,
		And_SD,		And_xx,		And_xxxx,
		Andi,
		Asl_D,		Asl_ii,		Asl_S1S2D,
		Asr_D,		Asr_ii,		Asr_S1S2D,
		Bcc_xxxx,	Bcc_xxx,	Bcc_Rn,
		Bchg_ea,	Bchg_aa,	Bchg_pp,		Bchg_qq,	Bchg_D,
		Bclr_ea,	Bclr_aa,	Bclr_pp,		Bclr_qq,	Bclr_D,
		Bra_xxxx,	Bra_xxx,	Bra_Rn,
		Brclr_ea,	Brclr_aa,	Brclr_pp,		Brclr_qq,	Brclr_S,
		BRKcc,
		Brset_ea,	Brset_aa,	Brset_pp,		Brset_qq,	Brset_S,
		BScc_xxxx,	BScc_xxx,	BScc_Rn,
		Bsclr_ea,	Bsclr_aa,	Bsclr_pp,		Bsclr_qq,	Bsclr_S,
		Bset_ea,	Bset_aa,	Bset_pp,		Bset_qq,	Bset_D,
		Bsr_xxxx,	Bsr_xxx,	Bsr_Rn,
		Bsset_ea,	Bsset_aa,	Bsset_pp,		Bsset_qq,	Bsset_S,
		Btst_ea,	Btst_aa,	Btst_pp,		Btst_qq,	Btst_D,
		Clb,
		Clr,
		Cmp_S1S2,	Cmp_xxS2,	Cmp_xxxxS2,
		Cmpm_S1S2,	Cmpu_S1S2,
		Debug,		Debugcc,
		Dec,
		Div,
		Dmac,
		Do_ea,		Do_aa,		Do_xxx,			Do_S,	DoForever,
		Dor_ea,		Dor_aa,		Dor_xxx,		Dor_S,	DorForever,
		Enddo,
		Eor_SD,		Eor_xx,		Eor_xxxx,
		Extract_S1S2,	Extract_CoS2,
		Extractu_S1S2,	Extractu_CoS2,
		Ifcc,		Ifcc_U,
		Illegal,
		Inc,
		Insert_S1S2,	Insert_CoS2,
		Jcc_xxx,	Jcc_ea,
		Jclr_ea,	Jclr_aa,	Jclr_pp,	Jclr_qq,	Jclr_S,
		Jmp_ea,		Jmp_xxx,
		Jscc_xxx,	Jscc_ea,
		Jsclr_ea,	Jsclr_aa,	Jsclr_pp,	Jsclr_qq,	Jsclr_S,
		Jset_ea,	Jset_aa,	Jset_pp,	Jset_qq,	Jset_S,
		Jsr_ea,		Jsr_xxx,
		Jsset_ea,	Jsset_aa,	Jsset_pp,	Jsset_qq,	Jsset_S,
		Lra_Rn,		Lra_xxxx,
		Lsl_D,		Lsl_ii,		Lsl_SD,
		Lsr_D,		Lsr_ii,		Lsr_SD,
		Lua_ea,		Lua_Rn,
		Mac_S1S2,	Mac_S,
		Maci_xxxx,
		Macsu,
		Macr_S1S2,	Macr_S,
		Macri_xxxx,
		Max,
		Maxm,
		Merge,
		Move_Nop,	Move_xx, Mover, Move_ea,
		Movex_ea,	Movex_aa,	Movex_Rnxxxx,	Movex_Rnxxx,
		Movexr_ea,	Movexr_A,
		Movey_ea,	Movey_aa,	Movey_Rnxxxx,	Movey_Rnxxx,
		Moveyr_ea,	Moveyr_A,
		Movel_ea,	Movel_aa,
		Movexy,
		Movec_ea,	Movec_aa,	Movec_S1D2,		Movec_xx,
		Movem_ea,
		Movem_aa,
		Movep_ppea,		Movep_Xqqea,	Movep_Yqqea,
		Movep_eapp,		Movep_eaqq,		Movep_Spp,
		Movep_SXqq,		Movep_SYqq,

		Mpy_S1S2D,		Mpy_SD,			Mpy_su,
		Mpyi,
		Mpyr_S1S2D,		Mpyr_SD,
		Mpyri,
		Neg,
		Nop,
		Norm,
		Normf,
		Not,
		Or_SD,	Or_xx,	Or_xxxx,
		Ori,
		Pflush, Pflushun, Pfree,
		Plock,	Plockr,
		Punlock,	Punlockr,
		Rep_ea,			Rep_aa,			Rep_xxx,			Rep_S,
		Reset,
		Rnd,
		Rol,
		Ror,
		Rti,
		Rts,
		Sbc,
		Stop,
		Sub_SD,	Sub_xx,	Sub_xxxx,
		Subl,
		Subr,
		Tcc_S1D1,			Tcc_S1D1S2D2,			Tcc_S2D2,
		Tfr,
		Trap,
		Trapcc,
		Tst,
		Vsl,
		Wait,

		ResolveCache,
		Parallel,

		InstructionCount
	};

	enum ExtensionWordTypes
	{
		None					= 0x0,
		EffectiveAddress		= 0x1,
		ImmediateData			= 0x2,
		AbsoluteAddressExt		= 0x4,
		PCRelativeAddressExt	= 0x8,

		EAandID					= EffectiveAddress | ImmediateData
	};

	enum OpcodeFlags
	{
		OpFlagBranch		= (1<<0),
		OpFlagLoop			= (1<<1),
		OpFlagCondition		= (1<<2),
		OpFlagCacheMod		= (1<<3),
		OpFlagPopPC			= (1<<4),
		OpFlagRepImmediate	= (1<<5),
		OpFlagRepDynamic	= (1<<6),
	};
}
