#pragma once
#include "dsp.h"
#include "utils.h"

#include "opcodetypes.h"

namespace dsp56k
{
	template <> inline void DSP::braOrBsr<DSP::Bra>(const int _offset)	{ setPC(pcCurrentInstruction + _offset); }
	template <> inline void DSP::braOrBsr<DSP::Bsr>(const int _offset)	{ jsr(pcCurrentInstruction + _offset); }

	template<Instruction Inst, DSP::BraMode Bmode> void DSP::braIfCC(const TWord op, const int offset)
	{
		if(checkCondition<Inst>(op))
			braOrBsr<Bmode>(offset);
	}

	template<Instruction Inst, DSP::BraMode BMode> void DSP::braIfCC(const TWord op)
	{
		const auto offset = relativeAddressOffset<Inst>(op);
		braIfCC<Inst,BMode>(op, offset);
	}

	template<Instruction Inst, DSP::BraMode BMode, DSP::ExpectedBitValue BitValue> void DSP::braIfBitTestMem(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		if(bitTestMemory<Inst>(op) == BitValue)
			braOrBsr<BMode>(addr);
	}

	template<Instruction Inst, DSP::BraMode BMode, DSP::ExpectedBitValue BitValue> void DSP::braIfBitTestDDDDDD(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		const auto d = getFieldValue<Inst,Field_DDDDDD>(op);
		const auto v = decode_dddddd_read(d).var;

		if( !bitTest<Inst>( op, v ) )
			braOrBsr<BMode>(addr);
	}

	// ________________________________
	//

	// Bcc
	inline void DSP::op_Bcc_xxxx(const TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bcc_xxxx>();
		if(checkCondition<Bcc_xxxx>(op))
			braOrBsr<Bra>(offset);
	}
	inline void DSP::op_Bcc_xxx(const TWord op)
	{
		braIfCC<Bcc_xxx, Bra>(op);
	}

	inline void DSP::op_Bcc_Rn(const TWord op)
	{
		braIfCC<Bcc_Rn, Bra>(op);
	}

	// Bra
	inline void DSP::op_Bra_xxxx(const TWord op)
	{
		const int offset = pcRelativeAddressExt<Bra_xxxx>();
		braOrBsr<Bra>(offset);
	}

	inline void DSP::op_Bra_xxx(const TWord op)
	{
		const auto offset = relativeAddressOffset<Bra_xxx>(op);
		braOrBsr<Bra>(offset);
	}
	inline void DSP::op_Bra_Rn(const TWord op)
	{
		const auto offset = relativeAddressOffset<Bra_Rn>(op);

		braOrBsr<Bra>(offset);
	}

	// Brclr
	inline void DSP::op_Brclr_ea(const TWord op)	{ braIfBitTestMem<Brclr_ea, Bra, BitClear>(op);	}
	inline void DSP::op_Brclr_aa(const TWord op)	{ braIfBitTestMem<Brclr_aa, Bra, BitClear>(op);	}
	inline void DSP::op_Brclr_pp(const TWord op)	{ braIfBitTestMem<Brclr_pp, Bra, BitClear>(op);	}
	inline void DSP::op_Brclr_qq(const TWord op)	{ braIfBitTestMem<Brclr_qq, Bra, BitClear>(op);	}
	inline void DSP::op_Brclr_S(const TWord op)		{ braIfBitTestDDDDDD<Brclr_S, Bra, BitClear>(op); }

	// Brset
	inline void DSP::op_Brset_ea(const TWord op)	{ braIfBitTestMem<Brset_ea, Bra, BitSet>(op); }
	inline void DSP::op_Brset_aa(const TWord op)	{ braIfBitTestMem<Brset_aa, Bra, BitSet>(op); }
	inline void DSP::op_Brset_pp(const TWord op)	{ braIfBitTestMem<Brset_pp, Bra, BitSet>(op); }
	inline void DSP::op_Brset_qq(const TWord op)	{ braIfBitTestMem<Brset_qq, Bra, BitSet>(op); }
	inline void DSP::op_Brset_S(const TWord op)		{ braIfBitTestDDDDDD<Brset_S, Bra, BitSet>(op); }

	// Bsclr
	inline void DSP::op_Bsclr_ea(const TWord op)	{ braIfBitTestMem<Brclr_ea, Bsr, BitClear>(op);	}
	inline void DSP::op_Bsclr_aa(const TWord op)	{ braIfBitTestMem<Brclr_aa, Bsr, BitClear>(op);	}
	inline void DSP::op_Bsclr_pp(const TWord op)	{ braIfBitTestMem<Brclr_pp, Bsr, BitClear>(op);	}
	inline void DSP::op_Bsclr_qq(const TWord op)	{ braIfBitTestMem<Brclr_qq, Bsr, BitClear>(op);	}
	inline void DSP::op_Bsclr_S(const TWord op)		{ braIfBitTestDDDDDD<Brclr_S, Bsr, BitClear>(op); }

	// Bsset
	inline void DSP::op_Bsset_ea(const TWord op)	{ braIfBitTestMem<Brset_ea, Bsr, BitSet>(op); }
	inline void DSP::op_Bsset_aa(const TWord op)	{ braIfBitTestMem<Brset_aa, Bsr, BitSet>(op); }
	inline void DSP::op_Bsset_pp(const TWord op)	{ braIfBitTestMem<Brset_pp, Bsr, BitSet>(op); }
	inline void DSP::op_Bsset_qq(const TWord op)	{ braIfBitTestMem<Brset_qq, Bsr, BitSet>(op); }
	inline void DSP::op_Bsset_S(const TWord op)		{ braIfBitTestDDDDDD<Brset_S, Bsr, BitSet>(op); }

	// BScc
	inline void DSP::op_BScc_xxxx(const TWord op)
	{
		const auto offset = pcRelativeAddressExt<BScc_xxxx>();
		if(checkCondition<BScc_xxxx>(op))
			braOrBsr<Bsr>(offset);
	}
	inline void DSP::op_BScc_xxx(const TWord op)
	{
		braIfCC<BScc_xxx, Bsr>(op);
	}

	inline void DSP::op_BScc_Rn(const TWord op)
	{
		braIfCC<BScc_Rn, Bsr>(op);
	}

	// Bsr
	inline void DSP::op_Bsr_xxxx(const TWord op)
	{
		const int offset = pcRelativeAddressExt<Bsr_xxxx>();
		braOrBsr<Bsr>(offset);
	}
	inline void DSP::op_Bsr_xxx(const TWord op)
	{
		const auto offset = relativeAddressOffset<Bsr_xxx>(op);
		braOrBsr<Bsr>(offset);
	}
	inline void DSP::op_Bsr_Rn(const TWord op)
	{
		const auto offset = relativeAddressOffset<Bsr_Rn>(op);
		braOrBsr<Bsr>(offset);
	}
}
