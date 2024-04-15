#pragma once
#include <cstdint>

#include "opcodeinfo.h"
#include "types.h"

namespace dsp56k
{
	struct OpcodeCycles
	{
		Instruction inst;

		uint32_t cycles;	/*	clock cycles for the normal case
								- All instructions fetched from the internal program memory
								- No interlocks with previous instructions
								- Addressing mode is the Post-Update mode (post-increment, post-decrement and post
								  offset by N) or the No-Update */

		uint32_t pru;		/* + pru. Pre-update specifies clock cycles added for using the
		                              pre-update addressing modes (pre-decrement and offset by N addressing modes) */

		uint32_t lab;		/* + lab. Long absolute specifies clock cycles added for using the Long Absolute Address */
		uint32_t lim;		/* + lim: Long immediate specifies clock cycles added for using the long immediate data addressing mode */
	};

	// those with 0 cycles are unspecified in the DSP56300 manual, we assume the cycle count is 1
	static constexpr OpcodeCycles g_cycles[] =
	{
		OpcodeCycles{Abs,				0, 0, 0, 0},
		OpcodeCycles{ADC,				0, 0, 0, 0},

		OpcodeCycles{Add_SD,			0, 0, 0, 0},
		OpcodeCycles{Add_xx,			1, 0, 0, 0},
		OpcodeCycles{Add_xxxx,			2, 0, 0, 0},

		OpcodeCycles{Addl,				0, 0, 0, 0},
		OpcodeCycles{Addr,				0, 0, 0, 0},

		OpcodeCycles{And_SD,			0, 0, 0, 0},
		OpcodeCycles{And_xx,			1, 0, 0, 0},
		OpcodeCycles{And_xxxx,			2, 0, 0, 0},

		OpcodeCycles{Andi,				3, 0, 0, 0},

		OpcodeCycles{Asl_D,				1, 0, 0, 0},
		OpcodeCycles{Asl_ii,			1, 0, 0, 0},
		OpcodeCycles{Asl_S1S2D,			1, 0, 0, 0},

		OpcodeCycles{Asr_D,				0, 0, 0, 0},
		OpcodeCycles{Asr_ii,			1, 0, 0, 0},
		OpcodeCycles{Asr_S1S2D,			1, 0, 0, 0},

		OpcodeCycles{Bcc_xxxx,			5, 0, 0, 0},
		OpcodeCycles{Bcc_xxx,			4, 0, 0, 0},
		OpcodeCycles{Bcc_Rn,			4, 0, 0, 0},

		OpcodeCycles{Bchg_ea,			2, 1, 1, 0},
		OpcodeCycles{Bchg_aa,			2, 0, 0, 0},
		OpcodeCycles{Bchg_pp,			2, 0, 0, 0},
		OpcodeCycles{Bchg_qq,			2, 0, 0, 0},
		OpcodeCycles{Bchg_D,			2, 0, 0, 0},

		OpcodeCycles{Bclr_ea,			2, 1, 1, 0},
		OpcodeCycles{Bclr_aa,			2, 0, 0, 0},
		OpcodeCycles{Bclr_pp,			2, 0, 0, 0},
		OpcodeCycles{Bclr_qq,			2, 0, 0, 0},
		OpcodeCycles{Bclr_D,			2, 0, 0, 0},

		OpcodeCycles{Bra_xxxx,			4, 0, 0, 0},
		OpcodeCycles{Bra_xxx,			4, 0, 0, 0},
		OpcodeCycles{Bra_Rn,			4, 0, 0, 0},

		OpcodeCycles{Brclr_ea,			5, 1, 0, 0},	// unspecified, derived from Brset
		OpcodeCycles{Brclr_aa,			5, 0, 0, 0},
		OpcodeCycles{Brclr_pp,			5, 0, 0, 0},
		OpcodeCycles{Brclr_qq,			5, 0, 0, 0},
		OpcodeCycles{Brclr_S,			5, 0, 0, 0},

		OpcodeCycles{BRKcc,				5, 0, 0, 0},

		OpcodeCycles{Brset_ea,			5, 1, 0, 0},	// There was nothing for pru but we have EA. See brset_qq
		OpcodeCycles{Brset_aa,			5, 0, 0, 0},
		OpcodeCycles{Brset_pp,			5, 0, 0, 0},
		OpcodeCycles{Brset_qq,			5, 0, 0, 0},	// here was a 1 for pru, but how should that be correct, there is no EA here
		OpcodeCycles{Brset_S,			5, 0, 0, 0},

		OpcodeCycles{BScc_xxxx,			5, 0, 0, 0},	// only aa is specified, assume its identical to BSR_xxxx
		OpcodeCycles{BScc_xxx,			4, 0, 0, 0},
		OpcodeCycles{BScc_Rn,			4, 0, 0, 0},

		OpcodeCycles{Bsclr_ea,			5, 1, 0, 0},
		OpcodeCycles{Bsclr_aa,			5, 0, 0, 0},
		OpcodeCycles{Bsclr_pp,			5, 0, 0, 0},
		OpcodeCycles{Bsclr_qq,			5, 0, 0, 0},
		OpcodeCycles{Bsclr_S,			5, 0, 0, 0},

		OpcodeCycles{Bset_ea,			2, 1, 1, 0},
		OpcodeCycles{Bset_aa,			2, 0, 0, 0},
		OpcodeCycles{Bset_pp,			2, 0, 0, 0},
		OpcodeCycles{Bset_qq,			2, 0, 0, 0},
		OpcodeCycles{Bset_D,			2, 0, 0, 0},

		OpcodeCycles{Bsr_xxxx,			5, 0, 0, 0},
		OpcodeCycles{Bsr_xxx,			4, 0, 0, 0},
		OpcodeCycles{Bsr_Rn,			4, 0, 0, 0},

		OpcodeCycles{Bsset_ea,			5, 1, 0, 0},
		OpcodeCycles{Bsset_aa,			5, 0, 0, 0},
		OpcodeCycles{Bsset_pp,			5, 0, 0, 0},
		OpcodeCycles{Bsset_qq,			5, 0, 0, 0},
		OpcodeCycles{Bsset_S,			5, 0, 0, 0},

		OpcodeCycles{Btst_ea,			2, 1, 1, 0},
		OpcodeCycles{Btst_aa,			2, 0, 0, 0},
		OpcodeCycles{Btst_pp,			2, 0, 0, 0},
		OpcodeCycles{Btst_qq,			2, 0, 0, 0},
		OpcodeCycles{Btst_D,			2, 0, 0, 0},

		OpcodeCycles{Clb,				1, 0, 0, 0},

		OpcodeCycles{Clr,				0, 0, 0, 0},

		OpcodeCycles{Cmp_S1S2,			0, 0, 0, 0},
		OpcodeCycles{Cmp_xxS2,			1, 0, 0, 0},
		OpcodeCycles{Cmp_xxxxS2,		2, 0, 0, 0},
		OpcodeCycles{Cmpm_S1S2,			0, 0, 0, 0},
		OpcodeCycles{Cmpu_S1S2,			1, 0, 0, 0},

		OpcodeCycles{Debug,				1, 0, 0, 0},
		OpcodeCycles{Debugcc,			5, 0, 0, 0},

		OpcodeCycles{Dec,				1, 0, 0, 0},
		OpcodeCycles{Div,				1, 0, 0, 0},
		OpcodeCycles{Dmac,				1, 0, 0, 0},

		OpcodeCycles{Do_ea,				5, 1, 0, 0},
		OpcodeCycles{Do_aa,				5, 0, 0, 0},
		OpcodeCycles{Do_xxx,			5, 0, 0, 0},
		OpcodeCycles{Do_S,				5, 0, 0, 0},

		OpcodeCycles{DoForever,			4, 0, 0, 0},

		OpcodeCycles{Dor_ea,			5, 1, 0, 0},
		OpcodeCycles{Dor_aa,			5, 0, 0, 0},
		OpcodeCycles{Dor_xxx,			5, 0, 0, 0},
		OpcodeCycles{Dor_S,				5, 0, 0, 0},

		OpcodeCycles{DorForever,		4, 0, 0, 0},	// unspecified, guessed from DoForever

		OpcodeCycles{Enddo,				1, 0, 0, 0},

		OpcodeCycles{Eor_SD,			0, 0, 0, 0},
		OpcodeCycles{Eor_xx,			1, 0, 0, 0},
		OpcodeCycles{Eor_xxxx,			2, 0, 0, 0},

		OpcodeCycles{Extract_S1S2,		1, 0, 0, 0},
		OpcodeCycles{Extract_CoS2,		2, 0, 0, 0},
		OpcodeCycles{Extractu_S1S2,		1, 0, 0, 0},
		OpcodeCycles{Extractu_CoS2,		2, 0, 0, 0},

		OpcodeCycles{Ifcc,				1, 0, 0, 0},
		OpcodeCycles{Ifcc_U,			0, 0, 0, 0},

		OpcodeCycles{Illegal,			5, 0, 0, 0},
		OpcodeCycles{Inc,				1, 0, 0, 0},

		OpcodeCycles{Insert_S1S2,		1, 0, 0, 0},
		OpcodeCycles{Insert_CoS2,		2, 0, 0, 0},

		OpcodeCycles{Jcc_xxx,			4, 0, 0, 0},
		OpcodeCycles{Jcc_ea,			4, 0, 0, 0},

		OpcodeCycles{Jclr_ea,			4, 1, 0, 0},
		OpcodeCycles{Jclr_aa,			4, 0, 0, 0},
		OpcodeCycles{Jclr_pp,			4, 0, 0, 0},
		OpcodeCycles{Jclr_qq,			4, 0, 0, 0},
		OpcodeCycles{Jclr_S,			4, 0, 0, 0},

		OpcodeCycles{Jmp_ea,			3, 1, 1, 0},
		OpcodeCycles{Jmp_xxx,			3, 0, 0, 0},

		OpcodeCycles{Jscc_xxx,			4, 0, 0, 0},
		OpcodeCycles{Jscc_ea,			4, 0, 0, 0},

		OpcodeCycles{Jsclr_ea,			4, 1, 0, 0},
		OpcodeCycles{Jsclr_aa,			4, 0, 0, 0},
		OpcodeCycles{Jsclr_pp,			4, 0, 0, 0},
		OpcodeCycles{Jsclr_qq,			4, 0, 0, 0},
		OpcodeCycles{Jsclr_S,			4, 0, 0, 0},

		OpcodeCycles{Jset_ea,			4, 1, 0, 0},	// unspecified, derived from Jclr
		OpcodeCycles{Jset_aa,			4, 0, 0, 0},
		OpcodeCycles{Jset_pp,			4, 0, 0, 0},
		OpcodeCycles{Jset_qq,			4, 0, 0, 0},
		OpcodeCycles{Jset_S,			4, 0, 0, 0},

		OpcodeCycles{Jsr_ea,			3, 1, 1, 0},
		OpcodeCycles{Jsr_xxx,			3, 0, 0, 0},

		OpcodeCycles{Jsset_ea,			4, 1, 0, 0},
		OpcodeCycles{Jsset_aa,			4, 0, 0, 0},
		OpcodeCycles{Jsset_pp,			4, 0, 0, 0},
		OpcodeCycles{Jsset_qq,			4, 0, 0, 0},
		OpcodeCycles{Jsset_S,			4, 0, 0, 0},

		OpcodeCycles{Lra_Rn,			3, 0, 0, 0},
		OpcodeCycles{Lra_xxxx,			3, 0, 0, 0},

		OpcodeCycles{Lsl_D,				0, 0, 0, 0},
		OpcodeCycles{Lsl_ii,			1, 0, 0, 0},
		OpcodeCycles{Lsl_SD,			1, 0, 0, 0},

		OpcodeCycles{Lsr_D,				0, 0, 0, 0},
		OpcodeCycles{Lsr_ii,			1, 0, 0, 0},
		OpcodeCycles{Lsr_SD,			1, 0, 0, 0},

		OpcodeCycles{Lua_ea,			3, 0, 0, 0},
		OpcodeCycles{Lua_Rn,			3, 0, 0, 0},

		OpcodeCycles{Mac_S1S2,			1, 0, 0, 0},
		OpcodeCycles{Mac_S,				1, 0, 0, 0},
		OpcodeCycles{Maci_xxxx,			2, 0, 0, 0},
		OpcodeCycles{Macsu,				0, 0, 0, 0},
		OpcodeCycles{Macr_S1S2,			0, 0, 0, 0},
		OpcodeCycles{Macr_S,			1, 0, 0, 0},
		OpcodeCycles{Macri_xxxx,		2, 0, 0, 0},
		OpcodeCycles{Max,				1, 0, 0, 0},
		OpcodeCycles{Maxm,				1, 0, 0, 0},
		OpcodeCycles{Merge,				1, 0, 0, 0},

		OpcodeCycles{Move_Nop,			1, 0, 0, 0},
		OpcodeCycles{Move_xx,			1, 0, 0, 0},
		OpcodeCycles{Mover,				1, 0, 0, 0},
		OpcodeCycles{Move_ea,			1, 0, 0, 0},

		OpcodeCycles{Movex_ea,			1, 1, 1, 1},
		OpcodeCycles{Movex_aa,			2, 0, 0, 0},
		OpcodeCycles{Movex_Rnxxxx,		3, 0, 0, 0},
		OpcodeCycles{Movex_Rnxxx,		2, 0, 0, 0},
		OpcodeCycles{Movexr_ea,			1, 1, 1, 1},
		OpcodeCycles{Movexr_A,			1, 1, 0, 0},

		OpcodeCycles{Movey_ea,			1, 1, 1, 1},
		OpcodeCycles{Movey_aa,			2, 0, 0, 0},
		OpcodeCycles{Movey_Rnxxxx,		3, 0, 0, 0},
		OpcodeCycles{Movey_Rnxxx,		2, 0, 0, 0},
		OpcodeCycles{Moveyr_ea,			1, 1, 1, 1},
		OpcodeCycles{Moveyr_A,			1, 1, 0, 0},

		OpcodeCycles{Movel_ea,			1, 1, 1, 0},
		OpcodeCycles{Movel_aa,			0, 0, 0, 0},

		OpcodeCycles{Movexy,			1, 0, 0, 0},

		OpcodeCycles{Movec_ea,			1, 1, 1, 1},
		OpcodeCycles{Movec_aa,			1, 0, 0, 0},
		OpcodeCycles{Movec_S1D2,		1, 0, 0, 0},
		OpcodeCycles{Movec_xx,			1, 0, 0, 0},

		OpcodeCycles{Movem_ea,			6, 1, 1, 0},
		OpcodeCycles{Movem_aa,			6, 1, 1, 0},

		OpcodeCycles{Movep_ppea,		2, 1, 1, 0},
		OpcodeCycles{Movep_Xqqea,		2, 1, 1, 0},
		OpcodeCycles{Movep_Yqqea,		2, 1, 1, 0},
		OpcodeCycles{Movep_eapp,		2, 1, 1, 0},
		OpcodeCycles{Movep_eaqq,		2, 1, 1, 0},
		OpcodeCycles{Movep_Spp,			1, 0, 0, 0},
		OpcodeCycles{Movep_SXqq,		1, 0, 0, 0},
		OpcodeCycles{Movep_SYqq,		1, 0, 0, 0},

		OpcodeCycles{Mpy_S1S2D,			1, 0, 0, 0},
		OpcodeCycles{Mpy_SD,			1, 0, 0, 0},
		OpcodeCycles{Mpy_su,			1, 0, 0, 0},
		OpcodeCycles{Mpyi,				2, 0, 0, 0},
		OpcodeCycles{Mpyr_S1S2D,		1, 0, 0, 0},
		OpcodeCycles{Mpyr_SD,			1, 0, 0, 0},
		OpcodeCycles{Mpyri,				2, 0, 0, 0},

		OpcodeCycles{Neg,				0, 0, 0, 0},

		OpcodeCycles{Nop,				1, 0, 0, 0},
		OpcodeCycles{Norm,				5, 0, 0, 0},
		OpcodeCycles{Normf,				1, 0, 0, 0},
		OpcodeCycles{Not,				0, 0, 0, 0},
		OpcodeCycles{Or_SD,				0, 0, 0, 0},
		OpcodeCycles{Or_xx,				1, 0, 0, 0},
		OpcodeCycles{Or_xxxx,			2, 0, 0, 0},
		OpcodeCycles{Ori,				3, 0, 0, 0},
		OpcodeCycles{Pflush,			1, 0, 0, 0},
		OpcodeCycles{Pflushun,			1, 0, 0, 0},
		OpcodeCycles{Pfree,				1, 0, 0, 0},
		OpcodeCycles{Plock,				2, 1, 1, 0},
		OpcodeCycles{Plockr,			4, 0, 0, 0},
		OpcodeCycles{Punlock,			2, 1, 1, 0},
		OpcodeCycles{Punlockr,			4, 0, 0, 0},

		OpcodeCycles{Rep_ea,			5, 1, 0, 0},
		OpcodeCycles{Rep_aa,			5, 0, 0, 0},
		OpcodeCycles{Rep_xxx,			5, 0, 0, 0},
		OpcodeCycles{Rep_S,				5, 0, 0, 0},

		OpcodeCycles{Reset,				7, 0, 0, 0},

		OpcodeCycles{Rnd,				0, 0, 0, 0},
		OpcodeCycles{Rol,				0, 0, 0, 0},
		OpcodeCycles{Ror,				0, 0, 0, 0},
		OpcodeCycles{Rti,				3, 0, 0, 0},
		OpcodeCycles{Rts,				3, 0, 0, 0},
		OpcodeCycles{Sbc,				0, 0, 0, 0},
		OpcodeCycles{Stop,			   10, 0, 0, 0},
		OpcodeCycles{Sub_SD,			0, 0, 0, 0},
		OpcodeCycles{Sub_xx,			1, 0, 0, 0},
		OpcodeCycles{Sub_xxxx,			2, 0, 0, 0},
		OpcodeCycles{Subl,				0, 0, 0, 0},
		OpcodeCycles{Subr,				0, 0, 0, 0},
		OpcodeCycles{Tcc_S1D1,			1, 0, 0, 0},
		OpcodeCycles{Tcc_S1D1S2D2,		1, 0, 0, 0},
		OpcodeCycles{Tcc_S2D2,			1, 0, 0, 0},
		OpcodeCycles{Tfr,				0, 0, 0, 0},
		OpcodeCycles{Trap,				9, 0, 0, 0},
		OpcodeCycles{Trapcc,			9, 0, 0, 0},
		OpcodeCycles{Tst,				0, 0, 0, 0},
		OpcodeCycles{Vsl,				1, 0, 0, 0},
		OpcodeCycles{Wait,			   10, 0, 0, 0},
		OpcodeCycles{ResolveCache,		0, 0, 0, 0},
		OpcodeCycles{Parallel,			0, 0, 0, 0}
	};

	uint32_t calcCycles(Instruction _inst, TWord _pc, TWord _op, TWord _extMemAddress, TWord _extMemWaitStates);
	uint32_t calcCycles(Instruction _instA, Instruction _instB, TWord _pc, TWord _op, TWord _extMemAddress, TWord _extMemWaitStates);
}
