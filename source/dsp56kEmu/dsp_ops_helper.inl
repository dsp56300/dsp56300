#pragma once
#include "dsp.h"
#include "opcodes.h"
#include "opcodetypes.h"

namespace dsp56k
{
	// Check Condition
	template <Instruction Inst> bool DSP::checkCondition(const TWord op) const
	{
		const TWord cccc = getFieldValue<Inst,Field_CCCC>(op);
		return decode_cccc( cccc );
	}

	// Effective Address
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

	// Relative Address
/*
	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaaaaaaaa>()>::type*> int DSP::relativeAddressOffset(const TWord op) const
	{
		return signextend<int,12>(getFieldValue<Inst, Field_aaaaaaaaaaaa>(op));
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaa>()>::type*>	int DSP::relativeAddressOffset(const TWord op) const
	{
		return signextend<int,6>(getFieldValue<Inst, Field_aaaaaa>(op));
	}
*/

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaa, Field_aaaaa>()>::type*>	int DSP::relativeAddressOffset(const TWord op) const
	{
		const TWord a = getFieldValue<Inst,Field_aaaa, Field_aaaaa>(op);
		return signextend<int,9>( a );
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_RRR>()>::type*> int DSP::relativeAddressOffset(const TWord op) const
	{
		const TWord r = getFieldValue<Inst,Field_RRR>(op);
		return signextend<int,24>(decode_RRR_read(r));
	}

	// Memory Read	
	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> TWord DSP::readMem(const TWord op)
	{
		return readMem<Inst>(op, getFieldValueMemArea<Inst>(op));
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> TWord DSP::readMem(const TWord op, EMemArea area)
	{
		const TWord mmmrrr = getFieldValue<Inst, Field_MMM, Field_RRR>(op);
		if (mmmrrr == MMM_ImmediateData)
			return immediateDataExt<Inst>();
		return memRead(area, decode_MMMRRR_read(mmmrrr));
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaaaaaaaa>()>::type*> TWord DSP::readMem(const TWord op, EMemArea area) const
	{
		return memRead(area, effectiveAddress<Inst>(op));
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaa>()>::type*> TWord DSP::readMem(const TWord op, EMemArea area) const
	{
		return memRead(area, getFieldValue<Inst, Field_aaaaaa>(op));
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaaaa, Field_S>()>::type*> TWord DSP::readMem(const TWord op) const
	{
		return readMem<Inst>(getFieldValue<Inst, Field_aaaaaa>(op), getFieldValueMemArea<Inst>(op));
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> TWord DSP::readMem(const TWord op) const
	{
		return memReadPeriphFFFF80(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_qqqqqq>(op));
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_pppppp, Field_S>()>::type*> TWord DSP::readMem(const TWord op) const
	{
		return memReadPeriphFFFFC0(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_pppppp>(op));
	}

	// Bit Manipulation & Access
	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_bbbbb, Field_S>()>::type*> bool DSP::bitTestMemory(TWord op)
	{
		return dsp56k::bittest<TWord>(readMem<Inst>(op), getBit<Inst>(op));
	}

	// Register Decode

}
