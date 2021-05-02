#pragma once

#include "dsp.h"
#include "opcodetypes.h"

namespace dsp56k
{
	constexpr TInstructionFunc g_opcodeFuncs[] =
	{
		&DSP::op_Abs,							// Abs
		&DSP::op_ADC,							// ADC 
		&DSP::op_Add_SD,						// Add_SD
		&DSP::op_Add_xx,						// Add_xx
		&DSP::op_Add_xxxx,						// Add_xxxx 
		&DSP::op_Addl,							// Addl 
		&DSP::op_Addr,							// Addr 
		&DSP::op_And_SD,						// And_SD 
		&DSP::op_And_xx,						// And_xx 
		&DSP::op_And_xxxx,						// And_xxxx 
		&DSP::op_Andi,							// Andi 
		&DSP::op_Asl_D,							// Asl_D 
		&DSP::op_Asl_ii,						// Asl_ii 
		&DSP::op_Asl_S1S2D,						// Asl_S1S2D 
		&DSP::op_Asr_D,							// Asr_D 
		&DSP::op_Asr_ii,						// Asr_ii 
		&DSP::op_Asr_S1S2D,						// Asr_S1S2D 
		&DSP::op_Bcc_xxxx,						// Bcc_xxxx 
		&DSP::op_Bcc_xxx,						// Bcc_xxx 
		&DSP::op_Bcc_Rn,						// Bcc_Rn 
		&DSP::op_Bchg_ea,						// Bchg_ea 
		&DSP::op_Bchg_aa,						// Bchg_aa 
		&DSP::op_Bchg_pp,						// Bchg_pp 
		&DSP::op_Bchg_qq,						// Bchg_qq 
		&DSP::op_Bchg_D,						// Bchg_D 
		&DSP::op_Bclr_ea,						// Bclr_ea 
		&DSP::op_Bclr_aa,						// Bclr_aa 
		&DSP::op_Bclr_pp,						// Bclr_pp 
		&DSP::op_Bclr_qq,						// Bclr_qq 
		&DSP::op_Bclr_D,						// Bclr_D 
		&DSP::op_Bra_xxxx,						// Bra_xxxx 
		&DSP::op_Bra_xxx,						// Bra_xxx 
		&DSP::op_Bra_Rn,						// Bra_Rn 
		&DSP::op_Brclr_ea,						// Brclr_ea 
		&DSP::op_Brclr_aa,						// Brclr_aa 
		&DSP::op_Brclr_pp,						// Brclr_pp 
		&DSP::op_Brclr_qq,						// Brclr_qq 
		&DSP::op_Brclr_S,						// Brclr_S 
		&DSP::op_BRKcc,							// BRKcc 
		&DSP::op_Brset_ea,						// Brset_ea 
		&DSP::op_Brset_aa,						// Brset_aa 
		&DSP::op_Brset_pp,						// Brset_pp 
		&DSP::op_Brset_qq,						// Brset_qq 
		&DSP::op_Brset_S,						// Brset_S 
		&DSP::op_BScc_xxxx,						// BScc_xxxx 
		&DSP::op_BScc_xxx,						// BScc_xxx 
		&DSP::op_BScc_Rn,						// BScc_Rn 
		&DSP::op_Bsclr_ea,						// Bsclr_ea 
		&DSP::op_Bsclr_aa,						// Bsclr_aa 
		&DSP::op_Bsclr_pp,						// Bsclr_pp 
		&DSP::op_Bsclr_qq,						// Bsclr_qq 
		&DSP::op_Bsclr_S,						// Bsclr_S 
		&DSP::op_Bset_ea,						// Bset_ea 
		&DSP::op_Bset_aa,						// Bset_aa 
		&DSP::op_Bset_pp,						// Bset_pp 
		&DSP::op_Bset_qq,						// Bset_qq 
		&DSP::op_Bset_D,						// Bset_D 
		&DSP::op_Bsr_xxxx,						// Bsr_xxxx 
		&DSP::op_Bsr_xxx,						// Bsr_xxx 
		&DSP::op_Bsr_Rn,						// Bsr_Rn 
		&DSP::op_Bsset_ea,						// Bsset_ea 
		&DSP::op_Bsset_aa,						// Bsset_aa 
		&DSP::op_Bsset_pp,						// Bsset_pp 
		&DSP::op_Bsset_qq,						// Bsset_qq 
		&DSP::op_Bsset_S,						// Bsset_S 
		&DSP::op_Btst_ea,						// Btst_ea 
		&DSP::op_Btst_aa,						// Btst_aa 
		&DSP::op_Btst_pp,						// Btst_pp 
		&DSP::op_Btst_qq,						// Btst_qq 
		&DSP::op_Btst_D,						// Btst_D 
		&DSP::op_Clb,							// Clb 
		&DSP::op_Clr,							// Clr 
		&DSP::op_Cmp_S1S2,						// Cmp_S1S2 
		&DSP::op_Cmp_xxS2,						// Cmp_xxS2 
		&DSP::op_Cmp_xxxxS2,					// Cmp_xxxxS2 
		&DSP::op_Cmpm_S1S2,						// Cmpm_S1S2 
		&DSP::op_Cmpu_S1S2,						// Cmpu_S1S2 
		&DSP::op_Debug,							// Debug 
		&DSP::op_Debugcc,						// Debugcc 
		&DSP::op_Dec,							// Dec 
		&DSP::op_Div,							// Div 
		&DSP::op_Dmac,							// Dmac 
		&DSP::op_Do_ea,							// Do_ea 
		&DSP::op_Do_aa,							// Do_aa 
		&DSP::op_Do_xxx,						// Do_xxx 
		&DSP::op_Do_S,							// Do_S 
		&DSP::op_DoForever,						// DoForever 
		&DSP::op_Dor_ea,						// Dor_ea 
		&DSP::op_Dor_aa,						// Dor_aa 
		&DSP::op_Dor_xxx,						// Dor_xxx 
		&DSP::op_Dor_S,							// Dor_S 
		&DSP::op_DorForever,					// DorForever 
		&DSP::op_Enddo,							// Enddo 
		&DSP::op_Eor_SD,						// Eor_SD 
		&DSP::op_Eor_xx,						// Eor_xx 
		&DSP::op_Eor_xxxx,						// Eor_xxxx 
		&DSP::op_Extract_S1S2,					// Extract_S1S2 
		&DSP::op_Extract_CoS2,					// Extract_CoS2 
		&DSP::op_Extractu_S1S2,					// Extractu_S1S2 
		&DSP::op_Extractu_CoS2,					// Extractu_CoS2 
		&DSP::op_Ifcc,							// Ifcc 
		&DSP::op_Ifcc_U,						// Ifcc_U 
		&DSP::op_Illegal,						// Illegal 
		&DSP::op_Inc,							// Inc 
		&DSP::op_Insert_S1S2,					// Insert_S1S2 
		&DSP::op_Insert_CoS2,					// Insert_CoS2 
		&DSP::op_Jcc_xxx,						// Jcc_xxx 
		&DSP::op_Jcc_ea,						// Jcc_ea 
		&DSP::op_Jclr_ea,						// Jclr_ea 
		&DSP::op_Jclr_aa,						// Jclr_aa 
		&DSP::op_Jclr_pp,						// Jclr_pp 
		&DSP::op_Jclr_qq,						// Jclr_qq 
		&DSP::op_Jclr_S,						// Jclr_S 
		&DSP::op_Jmp_ea,						// Jmp_ea 
		&DSP::op_Jmp_xxx,						// Jmp_xxx 
		&DSP::op_Jscc_xxx,						// Jscc_xxx 
		&DSP::op_Jscc_ea,						// Jscc_ea 
		&DSP::op_Jsclr_ea,						// Jsclr_ea 
		&DSP::op_Jsclr_aa,						// Jsclr_aa 
		&DSP::op_Jsclr_pp,						// Jsclr_pp 
		&DSP::op_Jsclr_qq,						// Jsclr_qq 
		&DSP::op_Jsclr_S,						// Jsclr_S 
		&DSP::op_Jset_ea,						// Jset_ea 
		&DSP::op_Jset_aa,						// Jset_aa 
		&DSP::op_Jset_pp,						// Jset_pp 
		&DSP::op_Jset_qq,						// Jset_qq 
		&DSP::op_Jset_S,						// Jset_S 
		&DSP::op_Jsr_ea,						// Jsr_ea 
		&DSP::op_Jsr_xxx,						// Jsr_xxx 
		&DSP::op_Jsset_ea,						// Jsset_ea 
		&DSP::op_Jsset_aa,						// Jsset_aa 
		&DSP::op_Jsset_pp,						// Jsset_pp 
		&DSP::op_Jsset_qq,						// Jsset_qq 
		&DSP::op_Jsset_S,						// Jsset_S 
		&DSP::op_Lra_Rn,						// Lra_Rn 
		&DSP::op_Lra_xxxx,						// Lra_xxxx 
		&DSP::op_Lsl_D,							// Lsl_D 
		&DSP::op_Lsl_ii,						// Lsl_ii 
		&DSP::op_Lsl_SD,						// Lsl_SD 
		&DSP::op_Lsr_D,							// Lsr_D 
		&DSP::op_Lsr_ii,						// Lsr_ii 
		&DSP::op_Lsr_SD,						// Lsr_SD 
		&DSP::op_Lua_ea,						// Lua_ea 
		&DSP::op_Lua_Rn,						// Lua_Rn 
		&DSP::op_Mac_S1S2,						// Mac_S1S2 
		&DSP::op_Mac_S,							// Mac_S 
		&DSP::op_Maci_xxxx,						// Maci_xxxx 
		&DSP::op_Macsu,							// Macsu 
		&DSP::op_Macr_S1S2,						// Macr_S1S2 
		&DSP::op_Macr_S,						// Macr_S 
		&DSP::op_Macri_xxxx,					// Macri_xxxx 
		&DSP::op_Max,							// Max 
		&DSP::op_Maxm,							// Maxm 
		&DSP::op_Merge,							// Merge 
		&DSP::op_Move_Nop,						// Move_Nop 
		&DSP::op_Move_xx,						// Move_xx 
		&DSP::op_Mover,							// Mover 
		&DSP::op_Move_ea,						// Move_ea 
		nullptr,								// Movex_ea 
		nullptr,								// Movex_aa 
		&DSP::op_Movex_Rnxxxx,					// Movex_Rnxxxx 
		&DSP::op_Movex_Rnxxx,					// Movex_Rnxxx 
		&DSP::op_Movexr_ea,						// Movexr_ea 
		&DSP::op_Movexr_A,						// Movexr_A 
		nullptr,								// Movey_ea 
		nullptr,								// Movey_aa 
		&DSP::op_Movey_Rnxxxx,					// Movey_Rnxxxx 
		&DSP::op_Movey_Rnxxx,					// Movey_Rnxxx 
		&DSP::op_Moveyr_ea,						// Moveyr_ea 
		&DSP::op_Moveyr_A,						// Moveyr_A 
		&DSP::op_Movel_ea,						// Movel_ea 
		&DSP::op_Movel_aa,						// Movel_aa 
		&DSP::op_Movexy,						// Movexy 
		&DSP::op_Movec_ea,						// Movec_ea 
		&DSP::op_Movec_aa,						// Movec_aa 
		&DSP::op_Movec_S1D2,					// Movec_S1D2 
		&DSP::op_Movec_xx,						// Movec_xx 
		&DSP::op_Movem_ea,						// Movem_ea 
		&DSP::op_Movem_aa,						// Movem_aa 
		&DSP::op_Movep_ppea,					// Movep_ppea 
		&DSP::op_Movep_Xqqea,					// Movep_Xqqea 
		&DSP::op_Movep_Yqqea,					// Movep_Yqqea 
		&DSP::op_Movep_eapp,					// Movep_eapp 
		&DSP::op_Movep_eaqq,					// Movep_eaqq 
		&DSP::op_Movep_Spp,						// Movep_Spp 
		&DSP::op_Movep_SXqq,					// Movep_SXqq 
		&DSP::op_Movep_SYqq,					// Movep_SYqq 
		&DSP::op_Mpy_S1S2D,						// Mpy_S1S2D 
		&DSP::op_Mpy_SD,						// Mpy_SD 
		&DSP::op_Mpy_su,						// Mpy_su 
		&DSP::op_Mpyi,							// Mpyi 
		&DSP::op_Mpyr_S1S2D,					// Mpyr_S1S2D 
		&DSP::op_Mpyr_SD,						// Mpyr_SD 
		&DSP::op_Mpyri,							// Mpyri 
		&DSP::op_Neg,							// Neg 
		&DSP::op_Nop,							// Nop 
		&DSP::op_Norm,							// Norm 
		&DSP::op_Normf,							// Normf 
		&DSP::op_Not,							// Not 
		&DSP::op_Or_SD,							// Or_SD 
		&DSP::op_Or_xx,							// Or_xx 
		&DSP::op_Or_xxxx,						// Or_xxxx 
		&DSP::op_Ori,							// Ori 
		&DSP::op_Pflush,						// Pflush 
		&DSP::op_Pflushun,						// Pflushun 
		&DSP::op_Pfree,							// Pfree 
		&DSP::op_Plock,							// Plock 
		&DSP::op_Plockr,						// Plockr 
		&DSP::op_Punlock,						// Punlock 
		&DSP::op_Punlockr,						// Punlockr 
		&DSP::op_Rep_ea,						// Rep_ea 
		&DSP::op_Rep_aa,						// Rep_aa 
		&DSP::op_Rep_xxx,						// Rep_xxx 
		&DSP::op_Rep_S,							// Rep_S 
		&DSP::op_Reset,							// Reset 
		&DSP::op_Rnd,							// Rnd 
		&DSP::op_Rol,							// Rol 
		&DSP::op_Ror,							// Ror 
		&DSP::op_Rti,							// Rti 
		&DSP::op_Rts,							// Rts 
		&DSP::op_Sbc,							// Sbc 
		&DSP::op_Stop,							// Stop 
		&DSP::op_Sub_SD,						// Sub_SD 
		&DSP::op_Sub_xx,						// Sub_xx 
		&DSP::op_Sub_xxxx,						// Sub_xxxx 
		&DSP::op_Subl,							// Subl 
		&DSP::op_subr,							// subr 
		&DSP::op_Tcc_S1D1,						// Tcc_S1D1 
		&DSP::op_Tcc_S1D1S2D2,					// Tcc_S1D1S2D2 
		&DSP::op_Tcc_S2D2,						// Tcc_S2D2 
		&DSP::op_Tfr,							// Tfr 
		&DSP::op_Trap,							// Trap 
		&DSP::op_Trapcc,						// Trapcc 
		&DSP::op_Tst,							// Tst 
		&DSP::op_Vsl,							// Vsl 
		&DSP::op_Wait,							// Wait 
		&DSP::op_ResolveCache,					// ResolveCache
		&DSP::op_Parallel,						// Parallel
	};

	constexpr size_t g_opcodeFuncsSize = sizeof(g_opcodeFuncs) / sizeof(g_opcodeFuncs[0]);
	static_assert(g_opcodeFuncsSize <= 256, "jump table too large");

	using TField = std::pair<Field,TWord>;	// Field + Field Value

	struct FieldPermutationType
	{
		constexpr explicit FieldPermutationType(const Instruction _instruction, const TField _field) noexcept
		: instruction(_instruction)
		, field(_field)
		{
		}

		const Instruction instruction;
		const TField field;
	};

	constexpr FieldPermutationType g_permutationTypes[] =
	{
		FieldPermutationType(Abs, {Field_d, 0}),
		FieldPermutationType(Abs, {Field_d, 1}),

		FieldPermutationType(Asl_D, {Field_d, 0}),
		FieldPermutationType(Asl_D, {Field_d, 1}),
		
		FieldPermutationType(And_SD, {Field_d, 0}),
		FieldPermutationType(And_SD, {Field_d, 1}),
		FieldPermutationType(And_SD, {Field_JJ, 0}),
		FieldPermutationType(And_SD, {Field_JJ, 1}),
		FieldPermutationType(And_SD, {Field_JJ, 2}),
		FieldPermutationType(And_SD, {Field_JJ, 3}),
/*
		FieldPermutationType(Movexy, {Field_MM, 0}),
		FieldPermutationType(Movexy, {Field_MM, 1}),
		FieldPermutationType(Movexy, {Field_MM, 2}),
		FieldPermutationType(Movexy, {Field_MM, 3}),

		FieldPermutationType(Movexy, {Field_RRR, 0}),
		FieldPermutationType(Movexy, {Field_RRR, 1}),
		FieldPermutationType(Movexy, {Field_RRR, 2}),
		FieldPermutationType(Movexy, {Field_RRR, 3}),
		FieldPermutationType(Movexy, {Field_RRR, 4}),
		FieldPermutationType(Movexy, {Field_RRR, 5}),
		FieldPermutationType(Movexy, {Field_RRR, 6}),
		FieldPermutationType(Movexy, {Field_RRR, 7}),

		FieldPermutationType(Movexy, {Field_mm, 0}),
		FieldPermutationType(Movexy, {Field_mm, 1}),
		FieldPermutationType(Movexy, {Field_mm, 2}),
		FieldPermutationType(Movexy, {Field_mm, 3}),

		FieldPermutationType(Movexy, {Field_rr, 0}),
		FieldPermutationType(Movexy, {Field_rr, 1}),
		FieldPermutationType(Movexy, {Field_rr, 2}),
		FieldPermutationType(Movexy, {Field_rr, 3}),
*/
		FieldPermutationType(Movexy, {Field_W, 0}),
		FieldPermutationType(Movexy, {Field_W, 1}),

		FieldPermutationType(Movexy, {Field_w, 0}),
		FieldPermutationType(Movexy, {Field_w, 1}),

		FieldPermutationType(Movexy, {Field_ee, 0}),
		FieldPermutationType(Movexy, {Field_ee, 1}),
		FieldPermutationType(Movexy, {Field_ee, 2}),
		FieldPermutationType(Movexy, {Field_ee, 3}),

		FieldPermutationType(Movexy, {Field_ff, 0}),
		FieldPermutationType(Movexy, {Field_ff, 1}),
		FieldPermutationType(Movexy, {Field_ff, 2}),
		FieldPermutationType(Movexy, {Field_ff, 3}),

		FieldPermutationType(Movex_ea, {Field_W, 0}),
		FieldPermutationType(Movex_ea, {Field_W, 1}),
		FieldPermutationType(Movex_aa, {Field_W, 0}),
		FieldPermutationType(Movex_aa, {Field_W, 1}),

		FieldPermutationType(Movey_ea, {Field_W, 0}),
		FieldPermutationType(Movey_ea, {Field_W, 1}),
		FieldPermutationType(Movey_aa, {Field_W, 0}),
		FieldPermutationType(Movey_aa, {Field_W, 1}),
	};

	constexpr size_t g_permutationTypeCount = sizeof(g_permutationTypes) / sizeof(g_permutationTypes[0]);

	struct FunctorAbs		{ template<TWord A>				constexpr TInstructionFunc get() const noexcept	{ return &DSP::opCE_Abs<A>;			} };
	struct FunctorAsl		{ template<TWord A>				constexpr TInstructionFunc get() const noexcept	{ return &DSP::opCE_Asl_D<A>;		} };
	struct FunctorAndSD		{ template<TWord A, TWord B>	constexpr TInstructionFunc get() const noexcept { return &DSP::opCE_And_SD<A,B>;	} };
	struct FunctorMovexy	{ template<TWord W, TWord w, TWord ee, TWord ff>	constexpr TInstructionFunc get() const noexcept { return &DSP::opCE_Movexy<W,w,ee,ff>;	} };

	struct FunctorMovex_ea	{ template<TWord W>	constexpr TInstructionFunc get() const noexcept { return &DSP::opCE_Movex_ea<W>;	} };
	struct FunctorMovex_aa	{ template<TWord W>	constexpr TInstructionFunc get() const noexcept { return &DSP::opCE_Movex_aa<W>;	} };
	struct FunctorMovey_ea	{ template<TWord W>	constexpr TInstructionFunc get() const noexcept { return &DSP::opCE_Movey_ea<W>;	} };
	struct FunctorMovey_aa	{ template<TWord W>	constexpr TInstructionFunc get() const noexcept { return &DSP::opCE_Movey_aa<W>;	} };

	constexpr TWord permutationCount(const Instruction _inst, const Field _field) noexcept
	{
		TWord result = 0;

		for (auto permutationType : g_permutationTypes)
		{
			if(permutationType.instruction == _inst && permutationType.field.first == _field)
				++result;
		}
		return result;
	}

	template<Instruction I, Field F>
	constexpr TWord permutationCount() noexcept
	{
		return permutationCount(I, F);
	}

	template<Instruction I>
	constexpr TWord permutationCount() noexcept
	{
		TWord result = 1;

		for(size_t f=0; f<Field_COUNT; ++f)
		{
			result *= std::max(static_cast<TWord>(1), permutationCount(I, static_cast<Field>(f)));
		}

		return result;
	}
	
	template<Instruction I, Field F, TWord _index>
	constexpr TWord permutationValue() noexcept
	{
		static_assert(getFieldInfoCE<I,F>().len > 0, "field not known for opcode");

		TWord index=0;

		for(size_t i=0; i<g_permutationTypeCount; ++i)
		{
			if(g_permutationTypes[i].instruction != I || g_permutationTypes[i].field.first != F)
				continue;

			if(index == _index)
			{
				return g_permutationTypes[i].field.second;
			}

			++index;
		}

		return 0;
	}

	template<Instruction I> constexpr TWord fieldCount()
	{
		TWord count = 0;
		auto lastField = Field_COUNT;

		for(auto p : g_permutationTypes)
		{
			if(p.instruction != I)
				continue;
			if(p.field.first != lastField)
			{
				++count;
				lastField = p.field.first;
			}
		}
		return count;
	}

	constexpr TWord totalPermutationCount()
	{
		TWord count = 0;
		auto lastField = Field_COUNT;

		for(auto p : g_permutationTypes)
		{
			if(p.field.first != lastField)
			{
				count += permutationCount(p.instruction, p.field.first);
				lastField = p.field.first;
			}
		}
		return count;
	}

	template<Instruction I>
	struct Permutation
	{
		Instruction inst;
		TInstructionFunc func;
		std::array<TField, fieldCount<I>()> fields;
	};

	template<Instruction I>
	using TPermutations = std::array<Permutation<I>,permutationCount<I>()>;

	template<Instruction I, TWord ...Fs> constexpr TWord permIndex(const Field FieldOfInterest, const TWord offset)
	{
		const TWord d[] = {Fs...};
		uint32_t mul = 1;
		for(size_t i=0; i<sizeof...(Fs); ++i)
		{
			if(d[i] == FieldOfInterest)
				return (offset / mul) % permutationCount(I,static_cast<Field>(d[i]));
			mul *= permutationCount(I,static_cast<Field>(d[i]));
		}
		return 0;
	}

	template<Field... Fields>
	struct FieldSequence
	{
		static constexpr size_t size() noexcept { return (sizeof...(Fields)); }
	};

	template<Instruction I, Field... Fs>
	auto constexpr permIndex(Field FieldOfInterest, TWord Offset, FieldSequence<Fs...>)
	{
	    return permIndex<I, Fs...>(FieldOfInterest, Offset);
	}

	using TPack = uint32_t;

	template<Instruction I, Field F, TWord Index> constexpr TPack packValues()
	{
		static_assert(F < 256, "field too large");
		static_assert(Index < 256, "index too large");
//		static_assert(permutationCount<I,F>() <= 256, "too many permutations");
		return (F) | (Index << 8);// | permutationCount<I,F>() << 16;
	}

	template<TPack Pack> constexpr Field unpackField()								{ return static_cast<Field>((Pack >> 0) & 0xff); }
	template<TPack Pack> constexpr TWord unpackIndex()								{ return static_cast<TWord>((Pack >> 8) & 0xff); }
//	template<Instruction I, TPack Pack> constexpr TWord unpackPermutationCount()	{ return static_cast<TWord>((Pack >> 16) & 0xffff); }	// Note: we can omit storing the permutationcount if we need more storage in the pack, will be more costly then but will work

	template<Instruction I, TPack Pack> constexpr TWord permutationValue()			{ return permutationValue<I, unpackField<Pack>(), unpackIndex<Pack>()>(); }

	template<typename Functor, Instruction I, TPack ...Pack> constexpr TInstructionFunc getPtr()
	{
		return Functor().template get< permutationValue<I, Pack>()...>();
	}

	template<Instruction I, TPack Pack> constexpr TField getTField()
	{
		return std::make_pair<Field,TWord>(unpackField<Pack>(), permutationValue<I, Pack>());
	}

	template<typename Functor, Instruction I, TPack ...Pack> constexpr Permutation<I> getPermutationFromPack()
	{
		return 
		{
			I,
			getPtr<Functor, I, Pack...>(),
			{getTField<I,Pack>()...},
		};
	}

	template<typename Functor, Instruction I, TWord Index, Field ...Fields> constexpr auto getPermutationFromPack(FieldSequence<Fields...> fields)
	{
		return getPermutationFromPack<Functor, I, packValues<I, Fields, permIndex<I>(Fields, Index, fields)>()...>();
	}

	template<typename Functor, Instruction I, TWord IdxLeft, TWord IdxRight, Field ...Fields>
	constexpr void getPermutationsRecursive(TPermutations<I>& _target)
	{
		if constexpr (IdxLeft == IdxRight)
		{
			_target[IdxLeft] = getPermutationFromPack<Functor, I, IdxLeft, Fields...>(FieldSequence<Fields...>());			
		}
		else if((IdxRight - IdxLeft) == 1)
		{
			_target[IdxLeft] = getPermutationFromPack<Functor, I, IdxLeft, Fields...>(FieldSequence<Fields...>());			
			_target[IdxRight] = getPermutationFromPack<Functor, I, IdxRight, Fields...>(FieldSequence<Fields...>());			
		}
		else
		{
			getPermutationsRecursive<Functor, I, IdxLeft, IdxLeft + (IdxRight-IdxLeft)/2, Fields...>(_target);
			getPermutationsRecursive<Functor, I, IdxLeft + (IdxRight-IdxLeft)/2, IdxRight, Fields...>(_target);
		}
	}

	template<typename Functor, Instruction I, Field ...Fields> constexpr TPermutations<I> getFuncs()
	{
		TPermutations<I> funcs{};
		static_assert((permutationCount<I>() & (permutationCount<I>()-1)) == 0, "permutation count needs to be a power of two");

		getPermutationsRecursive<Functor, I, 0, permutationCount<I>() - 1, Fields...>(funcs);

		return funcs;
	}
	/*
	static_assert(permutationCount(Abs, Field_d) == 2, "something wrong");
	static_assert(permutationCount(And_SD, Field_d) == 2, "something wrong");
	static_assert(permutationCount(And_SD, Field_JJ) == 4, "something wrong");
	static_assert(permutationCount<And_SD>() == 8, "something wrong");

	static_assert(permutationValue<Abs, Field_d,1>() == 1, "unexpected value for Field_d");
	static_assert(permutationValue<And_SD, Field_JJ,3>() == 3, "unexpected value for Field_JJ");
	*/

	class Jumptable
	{
	public:
		Jumptable()
		{
			m_jumpTable.reserve(g_opcodeFuncsSize);

			for(size_t i=0; i<g_opcodeFuncsSize; ++i)
				m_jumpTable.push_back(g_opcodeFuncs[i]);

			addPermutations(getFuncs<FunctorAbs, Abs, Field_d>());
			addPermutations(getFuncs<FunctorAndSD, And_SD, Field_d, Field_JJ>());
			addPermutations(getFuncs<FunctorMovexy, Movexy, Field_W, Field_w, Field_ee, Field_ff>());
			addPermutations(getFuncs<FunctorMovex_ea, Movex_ea, Field_W>());
			addPermutations(getFuncs<FunctorMovex_aa, Movex_aa, Field_W>());
			addPermutations(getFuncs<FunctorMovey_ea, Movey_ea, Field_W>());
			addPermutations(getFuncs<FunctorMovey_aa, Movey_aa, Field_W>());
		}

		const std::vector<TInstructionFunc>& jumptable() const { return m_jumpTable; }
		TWord resolve(const Instruction _inst, const TWord _op) const
		{
			const auto& perms = m_permutationInfo[_inst];

			if(perms.permutations.empty())
				return _inst;

			for(const auto& p : perms.permutations)
			{
				bool match = true;

				for(const auto& f : p.fields)
				{
					assert(hasField(_inst, f.first));

					const auto v = getFieldValue(_inst, f.first, _op);

					if(v != f.second)
					{
						match = false;
						break;
					}
				}
				if(match)
					return p.jumpTableIndex;
			}

			return _inst;
		}
	private:
		struct FieldValues
		{
			Field field;
			std::vector<TWord> values;
			std::vector<FieldValues> children;
		};

		struct PermutationInfo
		{
			TWord jumpTableIndex = 0;
			std::vector<TField> fields;
		};

		struct PermutationList
		{
			std::vector<PermutationInfo> permutations;
		};

		template<Instruction I>
		void addPermutations(const TPermutations<I>& _permutations)
		{
			auto& pl = m_permutationInfo[I];
			pl.permutations.clear();

			for (const Permutation<I>& p : _permutations)
			{
				PermutationInfo pi;

				pi.jumpTableIndex = (TWord)m_jumpTable.size();
				pi.fields.reserve(p.fields.size());

				for(const auto& f : p.fields)
					pi.fields.push_back(f);

				m_jumpTable.push_back(p.func);

				pl.permutations.emplace_back(pi);
			}
		}

		std::vector<TInstructionFunc> m_jumpTable;
		std::array<PermutationList, InstructionCount> m_permutationInfo;
	};
}
