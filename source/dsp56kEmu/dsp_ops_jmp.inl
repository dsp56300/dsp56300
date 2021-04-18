#pragma once

#include "dsp.h"
#include "types.h"

#include "dsp_ops_helper.inl"

namespace dsp56k
{
	template <> inline void DSP::jumpOrJSR<DSP::Jump>(const TWord _ea)	{ setPC(_ea); }
	template <> inline void DSP::jumpOrJSR<DSP::JSR>(const TWord _ea)	{ jsr(_ea); }

	template<Instruction Inst, DSP::JumpMode Jsr> void DSP::jumpIfCC(const TWord op, const TWord ea)
	{
		if(checkCondition<Inst>(op))
			jumpOrJSR<Jsr>(ea);
	}

	template<Instruction Inst, DSP::JumpMode Jsr> void DSP::jumpIfCC(const TWord op)
	{
		const auto ea = effectiveAddress<Inst>(op);
		jumpIfCC<Inst,Jsr>(op, ea);
	}

	template<Instruction Inst, DSP::JumpMode Jsr, DSP::ExpectedBitValue BitValue> void DSP::jumpIfBitTestMem(const TWord op)
	{
		const auto addr = absAddressExt<Inst>();

		if(bitTestMemory<Inst>(op) == BitValue)
			jumpOrJSR<Jsr>(addr);
	}

	template<Instruction Inst, DSP::JumpMode Jsr, DSP::ExpectedBitValue BitValue> void DSP::jumpIfBitTestDDDDDD(const TWord op)
	{
		const auto addr = absAddressExt<Inst>();

		const auto d = getFieldValue<Inst,Field_DDDDDD>(op);
		const auto v = decode_dddddd_read(d).var;

		if( bitTest<Inst>( op, v ) == BitValue )
			jumpOrJSR<Jsr>(addr);
	}

	// ____________________________________________
	//

	// Jcc
	inline void DSP::op_Jcc_xxx(const TWord op)
	{
		jumpIfCC<Jcc_xxx, Jump>(op);
	}
	inline void DSP::op_Jcc_ea(const TWord op)
	{
		jumpIfCC<Jcc_ea, Jump>(op);
	}

	// Jclr
	inline void DSP::op_Jclr_ea(const TWord op)
	{
		jumpIfBitTestMem<Jclr_ea, Jump, BitClear>(op);
	}
	inline void DSP::op_Jclr_aa(const TWord op)
	{
		jumpIfBitTestMem<Jclr_aa, Jump, BitClear>(op);
	}
	inline void DSP::op_Jclr_pp(const TWord op)
	{
		jumpIfBitTestMem<Jclr_pp, Jump, BitClear>(op);
	}
	inline void DSP::op_Jclr_qq(const TWord op)
	{
		jumpIfBitTestMem<Jclr_qq, Jump, BitClear>(op);
	}
	inline void DSP::op_Jclr_S(const TWord op)
	{
		jumpIfBitTestDDDDDD<Jclr_S, Jump, BitClear>(op);
	}

	// Jmp
	inline void DSP::op_Jmp_ea(const TWord op)
	{
		setPC(effectiveAddress<Jmp_ea>(op));
	}
	inline void DSP::op_Jmp_xxx(const TWord op)
	{
		setPC(effectiveAddress<Jmp_xxx>(op));
	}
	
	// Jscc
	inline void DSP::op_Jscc_xxx(const TWord op)
	{
		jumpIfCC<Jscc_xxx, JSR>(op);
	}
	inline void DSP::op_Jscc_ea(const TWord op)
	{
		jumpIfCC<Jscc_ea, JSR>(op);
	}

	// Jsclr
	inline void DSP::op_Jsclr_ea(const TWord op)
	{
		jumpIfBitTestMem<Jsclr_ea, JSR, BitClear>(op);
	}
	inline void DSP::op_Jsclr_aa(const TWord op)
	{
		jumpIfBitTestMem<Jsclr_aa, JSR, BitClear>(op);
	}
	inline void DSP::op_Jsclr_pp(const TWord op)
	{
		jumpIfBitTestMem<Jsclr_pp, JSR, BitClear>(op);
	}
	inline void DSP::op_Jsclr_qq(const TWord op)
	{
		jumpIfBitTestMem<Jsclr_qq, JSR, BitClear>(op);
	}
	inline void DSP::op_Jsclr_S(const TWord op)
	{
		jumpIfBitTestDDDDDD<Jsclr_S, JSR, BitClear>(op);
	}

	// Jset
	inline void DSP::op_Jset_ea(const TWord op)
	{
		jumpIfBitTestMem<Jset_ea, Jump, BitSet>(op);
	}
	inline void DSP::op_Jset_aa(const TWord op)
	{
		jumpIfBitTestMem<Jset_aa, Jump, BitSet>(op);
	}
	inline void DSP::op_Jset_pp(const TWord op)
	{
		jumpIfBitTestMem<Jset_pp, Jump, BitSet>(op);
	}
	inline void DSP::op_Jset_qq(const TWord op)
	{
		jumpIfBitTestMem<Jset_qq, Jump, BitSet>(op);
	}
	inline void DSP::op_Jset_S(const TWord op)
	{
		jumpIfBitTestDDDDDD<Jset_S, Jump, BitSet>(op);
	}

	// Jsr
	inline void DSP::op_Jsr_ea(const TWord op)
	{
		jsr(effectiveAddress<Jsr_ea>(op));
	}
	inline void DSP::op_Jsr_xxx(const TWord op)
	{
		jsr(effectiveAddress<Jsr_xxx>(op));
	}

	// Jsset
	inline void DSP::op_Jsset_ea(const TWord op)
	{
		jumpIfBitTestMem<Jsset_ea, JSR, BitSet>(op);
	}
	inline void DSP::op_Jsset_aa(const TWord op)
	{
		jumpIfBitTestMem<Jsset_aa, JSR, BitSet>(op);
	}
	inline void DSP::op_Jsset_pp(const TWord op)
	{
		jumpIfBitTestMem<Jsset_pp, JSR, BitSet>(op);
	}
	inline void DSP::op_Jsset_qq(const TWord op)
	{
		jumpIfBitTestMem<Jsset_qq, JSR, BitSet>(op);
	}
	inline void DSP::op_Jsset_S(const TWord op)
	{
		jumpIfBitTestDDDDDD<Jsset_S, JSR, BitSet>(op);
	}
}
