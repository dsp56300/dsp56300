#pragma once

#include "jitops.h"

#ifdef HAVE_ARM64
#include "jitops_jmp_aarch64.inl"
#else
#include "jitops_jmp_x64.inl"
#endif

#include "jitops_helper.inl"
#include "opcodecycles.h"

namespace dsp56k
{
	// ________________________________________________
	// BRA (relative jumps)

	template<Instruction Inst, BraMode BMode, ExpectedBitValue BitValue> void JitOps::braIfBitTestMem(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		DspValue a(m_block, addr, DspValue::Immediate24);

		DSPReg pc(m_block, PoolReg::DspPC, true, true);
		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			bitTestMemory<Inst>(op, BitValue, _toFalse);
		}, [&]()
		{
			braOrBsr<BMode>(a);
		}, BMode == Bsr);
	}

	template<Instruction Inst, BraMode BMode, ExpectedBitValue BitValue> void JitOps::braIfBitTestDDDDDD(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();
		DspValue a(m_block, addr, DspValue::Immediate24);

		const auto dddddd = getFieldValue<Inst,Field_DDDDDD>(op);

		DspValue r(m_block);
		decode_dddddd_read(r, dddddd);

		DSPReg pc(m_block, PoolReg::DspPC, true, true);
		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			bitTest<Inst>(op, r, BitValue, _toFalse);
		}, [&]()
		{
			braOrBsr<BMode>(a);
		}, BMode == Bsr);
	}

	template<Instruction Inst, BraMode Bmode> void JitOps::braIfCC(const TWord op, DspValue& offset)
	{
		const DSPReg pc(m_block, PoolReg::DspPC, true, true);

		if constexpr (Bmode == Bra)
		{
			const auto cccc = getFieldValue<Inst,Field_CCCC>(op);

			if(offset.isImmediate())
			{
				const RegGP newPC(m_block);
				m_asm.mov(r32(newPC), asmjit::Imm(m_pcCurrentOp + offset.imm24()));
				m_asm.cmov(decode_cccc(cccc), r32(pc), r32(newPC));
			}
			else
			{
				m_asm.add(r32(offset), m_pcCurrentOp);
				m_asm.cmov(decode_cccc(cccc), r32(pc), r32(offset));
			}
		}
		else
		{
			checkCondition<Inst>(op, [&]()
			{
				braOrBsr<Bmode>(offset);
			}, Bmode == Bsr);
		}
	}

	// ________________________________________________
	// JMP (absolute jumps)

	template <> inline void JitOps::jumpOrJSR<Jump>(DspValue& _ea)			{ jmp(_ea); }
	template <> inline void JitOps::jumpOrJSR<JSR>(DspValue& _ea)			{ jsr(_ea); }

	template<Instruction Inst, JumpMode Bmode> void JitOps::jumpIfCC(const TWord op, DspValue& offset)
	{
		const DSPReg pc(m_block, PoolReg::DspPC, true, true);

		if constexpr(Bmode == Jump)
		{
			const auto cccc = getFieldValue<Inst,Field_CCCC>(op);

			if(offset.isImmediate())
			{
				const RegGP newPC(m_block);
				m_asm.mov(r32(newPC), asmjit::Imm(offset.imm24()));
				m_asm.cmov(decode_cccc(cccc), r32(pc), r32(newPC));
			}
			else
			{
				m_asm.cmov(decode_cccc(cccc), r32(pc), r32(offset));
			}
		}
		else
		{
			checkCondition<Inst>(op, [&]()
			{
				jumpOrJSR<Bmode>(offset);
			}, Bmode == JSR);
		}
	}

	template<Instruction Inst, JumpMode Bmode, ExpectedBitValue BitValue> void JitOps::jumpIfBitTestMem(const TWord _op)
	{
		const auto addr = absAddressExt<Inst>();
		DspValue a(m_block, addr, DspValue::Immediate24);

		DSPReg pc(m_block, PoolReg::DspPC, true, true);
		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			bitTestMemory<Inst>(_op, BitValue, _toFalse);
		}, [&]()
		{
			jumpOrJSR<Bmode>(a);
		}, Bmode == JSR);
	}

	template<Instruction Inst, JumpMode Bmode, ExpectedBitValue BitValue> void JitOps::jumpIfBitTestDDDDDD(const TWord op)
	{
		const auto addr = absAddressExt<Inst>();
		DspValue a(m_block, addr, DspValue::Immediate24);

		const auto dddddd = getFieldValue<Inst,Field_DDDDDD>(op);

		DSPReg pc(m_block, PoolReg::DspPC, true, true);
		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			DspValue r(m_block);
			decode_dddddd_read(r, dddddd);
			bitTest<Inst>(op, r, BitValue, _toFalse);
		}, [&]()
		{
			jumpOrJSR<Bmode>(a);
		}, Bmode == JSR);
	}
}
