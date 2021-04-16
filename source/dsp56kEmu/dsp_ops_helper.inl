#pragma once
#include "dsp.h"
#include "opcodes.h"
#include "opcodetypes.h"

namespace dsp56k
{
	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> TWord DSP::effectiveAddress(const TWord op)
	{
		const TWord mmmrrr = getFieldValue<Inst, Field_MMM, Field_RRR>(op);
		return decode_MMMRRR_read(mmmrrr);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaaaaaaaa>()>::type*> TWord DSP::effectiveAddress(const TWord op) const
	{
		return getFieldValue<Inst, Field_aaaaaaaaaaaa>(op);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaa>()>::type*>	TWord DSP::effectiveAddress(const TWord op) const
	{
		return getFieldValue<Inst, Field_aaaaaa>(op);
	}	
}
