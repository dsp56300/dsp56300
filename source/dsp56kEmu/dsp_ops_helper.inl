#pragma once
#include "dsp.h"
#include "opcodes.h"
#include "opcodetypes.h"

namespace dsp56k
{
	// Check Condition
	template <Instruction Inst> int DSP::checkCondition(const TWord op) const
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

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_a, Field_RRR>() && hasField<Inst, Field_aaaaaa>()>::type*> TWord DSP::effectiveAddress(const TWord op) const
	{
		return getFieldValue<Inst, Field_aaaaaa>(op);
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaaaa, Field_a, Field_RRR>()>::type*> TWord DSP::effectiveAddress(const TWord op) const
	{
		const TWord aaaaaaa	= getFieldValue<Inst,Field_aaaaaa, Field_a>(op);
		const TWord rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = signextend<int,7>(aaaaaaa);
		return decode_RRR_read( rrr, shortDisplacement );
	}

	// Relative Address
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
	template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> TWord DSP::readMem(const TWord op)
	{
		return readMem<Inst>(op, getFieldValueMemArea<Inst>(op));
	}

	template <Instruction Inst, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type*> TWord DSP::readMem(const TWord op, EMemArea area)
	{
		const TWord mmmrrr = getFieldValue<Inst, Field_MMM, Field_RRR>(op);

		if (mmmrrr == MMM_ImmediateData)
			return immediateDataExt<Inst>();

		const auto ea = decode_MMMRRR_read(mmmrrr);

		// TODO: I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing.
		if(ea >= XIO_Reserved_High_First)
			return memReadPeriph(area, ea);
		return memRead(area, ea);
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

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> TWord DSP::readMem(const TWord op) const
	{
		return memReadPeriphFFFF80(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_qqqqqq>(op));
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type*> TWord DSP::readMem(const TWord op) const
	{
		return memReadPeriphFFFFC0(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_pppppp>(op));
	}

	// Memory Write	
	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type*> void DSP::writeMem(const TWord op, const TWord value)
	{
		return writeMem<Inst>(op, getFieldValueMemArea<Inst>(op), value);
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		const TWord mmmrrr = getFieldValue<Inst, Field_MMM, Field_RRR>(op);

		assert(mmmrrr != MMM_ImmediateData && "can't write to immediate data");

		const auto ea = decode_MMMRRR_read(mmmrrr);

		// TODO: I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing.
		if(ea >= XIO_Reserved_High_First)
			memWritePeriph(area, ea, value);
		else
			memWrite(area, ea, value);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaaaaaaaa>()>::type*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		memWrite(area, effectiveAddress<Inst>(op), value);
	}

	template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaa>()>::type*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		memWrite(area, getFieldValue<Inst, Field_aaaaaa>(op), value);
	}

	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaaaa, Field_S>()>::type*> void DSP::writeMem(const TWord op, const TWord value)
	{
		writeMem<Inst>(getFieldValue<Inst, Field_aaaaaa>(op), getFieldValueMemArea<Inst>(op), value);
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type*> void DSP::writeMem(const TWord op, const TWord value)
	{
		memWritePeriphFFFF80(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_qqqqqq>(op), value);
	}

	template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type*> void DSP::writeMem(const TWord op, const TWord value)
	{
		memWritePeriphFFFFC0(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_pppppp>(op), value);
	}

	// Bit Manipulation & Access
	template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_bbbbb, Field_S>()>::type*> bool DSP::bitTestMemory(TWord op)
	{
		return dsp56k::bittest<TWord>(readMem<Inst>(op), getBit<Inst>(op));
	}

	// Register Decode

}
