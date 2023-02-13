#pragma once

#include "jitops.h"
#include "jitops_mem.inl"

#include "asmjit/core/operand.h"

namespace dsp56k
{
	template <Instruction Inst, std::enable_if_t<hasField<Inst, Field_bbbbb>()>*> void JitOps::bitTest(TWord op, DspValue& _value, const ExpectedBitValue _bitValue, const asmjit::Label _skip) const
	{
		const auto b = getBit<Inst>(op);

#ifdef HAVE_ARM64
		const auto bit = asmjit::Imm(b);
		if (_bitValue == BitSet)
			m_asm.tbz(_value.get(), bit, _skip);
		else if (_bitValue == BitClear)
			m_asm.tbnz(_value.get(), bit, _skip);
#else
		m_asm.bitTest(_value.get(), b);

		if (_bitValue == BitSet)
			m_asm.jz(_skip);
		else if (_bitValue == BitClear)
			m_asm.jnz(_skip);
#endif
	}

	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_bbbbb, Field_S>()>*> void JitOps::bitTestMemory(const TWord _op, const ExpectedBitValue _bitValue, const asmjit::Label _skip)
	{
		DspValue r(m_block);
		readMem<Inst>(r, _op);

		bitTest<Inst>(_op, r, _bitValue, _skip);
	}
	template <Instruction Inst, std::enable_if_t<hasField<Inst, Field_bbbbb>()>*> JitCondCode JitOps::bitTest(TWord op, DspValue& _value, const ExpectedBitValue _bitValue) const
	{
		const auto b = getBit<Inst>(op);
		m_asm.bitTest(_value.get(), b);
		return _bitValue == BitSet ? JitCondCode::kNotZero : JitCondCode::kZero;
	}

	template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_bbbbb, Field_S>()>*> JitCondCode JitOps::bitTestMemory(const TWord _op, const ExpectedBitValue _bitValue)
	{
		DspValue r(m_block);
		readMem<Inst>(r, _op);

		return bitTest<Inst>(_op, r, _bitValue);
	}
}
