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
	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_MMM, Field_RRR>()>*> TWord DSP::effectiveAddress(const TWord op)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(op);
		return decode_MMMRRR_read(mmm, rrr);
	}

	template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_aaaaaaaaaaaa>()>*> TWord DSP::effectiveAddress(const TWord op) const
	{
		return getEffectiveAddress<Inst>(op);
	}

	template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_a, Field_RRR>() && hasFieldT<Inst, Field_aaaaaa>()>*> TWord DSP::effectiveAddress(const TWord op) const
	{
		return getEffectiveAddress<Inst>(op);
	}

	template <Instruction Inst, std::enable_if_t<has3Fields<Inst, Field_aaaaaa, Field_a, Field_RRR>()>*> TWord DSP::effectiveAddress(const TWord op) const
	{
		const TWord aaaaaaa	= getFieldValue<Inst,Field_aaaaaa, Field_a>(op);
		const TWord rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = signextend<int,7>(aaaaaaa);
		return decode_RRR_read( rrr, shortDisplacement );
	}

	// Relative Address
	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_aaaa, Field_aaaaa>()>*>	int DSP::relativeAddressOffset(const TWord op) const
	{
		return getRelativeAddressOffset<Inst>(op);
	}

	template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_RRR>()>*> int DSP::relativeAddressOffset(const TWord op) const
	{
		const TWord r = getFieldValue<Inst,Field_RRR>(op);
		return signextend<int,24>(decode_RRR_read(r));
	}

	// Memory Read	
	template <Instruction Inst, std::enable_if_t<!hasFieldT<Inst,Field_s>() && has3Fields<Inst, Field_MMM, Field_RRR, Field_S>()>*> TWord DSP::readMem(const TWord op)
	{
		return readMem<Inst>(op, getFieldValueMemArea<Inst>(op));
	}

	template <Instruction Inst, std::enable_if_t<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>*> TWord DSP::readMem(const TWord op, EMemArea area)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(op);

		if ((mmm << 3 | rrr) == MMMRRR_ImmediateData)
			return immediateDataExt<Inst>();

		const auto ea = decode_MMMRRR_read(mmm, rrr);

		// TODO: I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing.
		if(isPeripheralAddress(ea))
			return memReadPeriph(area, ea, Inst);
		return memRead(area, ea);
	}

	template <Instruction Inst, TWord MMM, std::enable_if_t<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>*> TWord DSP::readMem(const TWord op, EMemArea area)
	{
		const TWord rrr = getFieldValue<Inst, Field_RRR>(op);

		if constexpr (MMM == MMM_ImmediateData)
		{
			if(rrr == RRR_ImmediateData)
				return immediateDataExt<Inst>();
		}

		const auto ea = decode_MMMRRR_read<MMM>(rrr);

		// TODO: I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing.
		if(isPeripheralAddress(ea))
			return memReadPeriph(area, ea, Inst);
		return memRead(area, ea);
	}

	template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_aaaaaaaaaaaa>()>*> TWord DSP::readMem(const TWord op, EMemArea area) const
	{
		return memRead(area, effectiveAddress<Inst>(op));
	}

	template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_aaaaaa>()>*> TWord DSP::readMem(const TWord op, EMemArea area) const
	{
		return memRead(area, getFieldValue<Inst, Field_aaaaaa>(op));
	}

	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_aaaaaa, Field_S>()>*> TWord DSP::readMem(const TWord op) const
	{
		return readMem<Inst>(op, getFieldValueMemArea<Inst>(op));
	}

	template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>*> TWord DSP::readMem(const TWord op) const
	{
		return memReadPeriphFFFF80(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_qqqqqq>(op), Inst);
	}

	template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>*> TWord DSP::readMem(const TWord op) const
	{
		return memReadPeriphFFFFC0(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_pppppp>(op), Inst);
	}

	// Memory Write	
	template <Instruction Inst, std::enable_if_t<has3Fields<Inst, Field_MMM, Field_RRR, Field_S>()>*> void DSP::writeMem(const TWord op, const TWord value)
	{
		return writeMem<Inst>(op, getFieldValueMemArea<Inst>(op), value);
	}

	template <Instruction Inst, TWord MMM, std::enable_if_t<hasFields<Inst, Field_MMM, Field_RRR>()>*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		const TWord rrr = getFieldValue<Inst, Field_RRR>(op);

		assert((MMM << 3 | rrr) != MMMRRR_ImmediateData && "can't write to immediate data");

		const auto ea = decode_MMMRRR_read<MMM>(rrr);

		// TODO: I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing.
		if(isPeripheralAddress(ea))
			memWritePeriph(area, ea, value);
		else
			memWrite(area, ea, value);
	}

	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_MMM, Field_RRR>()>*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		const TWord mmm = getFieldValue<Inst, Field_MMM>(op);
		const TWord rrr = getFieldValue<Inst, Field_RRR>(op);

		assert((mmm << 3 | rrr) != MMMRRR_ImmediateData && "can't write to immediate data");

		const auto ea = decode_MMMRRR_read(mmm, rrr);

		// TODO: I don't like this. There are special instructions to access peripherals, but the decoding allows to access peripherals with regular addressing.
		if(isPeripheralAddress(ea))
			memWritePeriph(area, ea, value);
		else
			memWrite(area, ea, value);
	}

	template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_aaaaaaaaaaaa>()>*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		memWrite(area, effectiveAddress<Inst>(op), value);
	}

	template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_aaaaaa>()>*> void DSP::writeMem(const TWord op, EMemArea area, const TWord value)
	{
		memWrite(area, getFieldValue<Inst, Field_aaaaaa>(op), value);
	}

	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_aaaaaa, Field_S>()>*> void DSP::writeMem(const TWord op, const TWord value)
	{
		writeMem<Inst>(op, getFieldValueMemArea<Inst>(op), value);
	}

	template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>*> void DSP::writeMem(const TWord op, const TWord value)
	{
		memWritePeriphFFFF80(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_qqqqqq>(op), value);
	}

	template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>*> void DSP::writeMem(const TWord op, const TWord value)
	{
		memWritePeriphFFFFC0(getFieldValueMemArea<Inst>(op), getFieldValue<Inst, Field_pppppp>(op), value);
	}

	// Bit Manipulation & Access
	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_bbbbb, Field_S>()>*> bool DSP::bitTestMemory(TWord op)
	{
		return dsp56k::bittest<TWord>(readMem<Inst>(op), getBit<Inst>(op));
	}

	// Register Decode

}
