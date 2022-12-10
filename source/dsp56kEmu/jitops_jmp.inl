#pragma once

#include "jitops.h"

#ifdef HAVE_ARM64
#include "jitops_jmp_aarch64.inl"
#else
#include "jitops_jmp_x64.inl"
#endif

#include "jitops_helper.inl"

namespace dsp56k
{
	// ________________________________________________
	// BRA (relative jumps)

	template<Instruction Inst, BraMode BMode, ExpectedBitValue BitValue> void JitOps::braIfBitTestMem(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		DspValue a(m_block, addr, DspValue::Immediate24);

		DSPReg pc(m_block, JitDspRegPool::DspPC, true, true);
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

		DSPReg pc(m_block, JitDspRegPool::DspPC, true, true);
		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			bitTest<Inst>(op, r, BitValue, _toFalse);
		}, [&]()
		{
			braOrBsr<BMode>(a);
		}, BMode == Bsr);
	}

	template<Instruction Inst, ExpectedBitValue BitValue> void JitOps::esaiFrameSyncSpinloop(TWord op)
	{
		// If the DSP is spinlooping while waiting for the ESAI frame sync to flip, we fast-forward the DSP instructions to make it happen ASAP
		const auto bit = getBit<Inst>(op);
		const auto addr = getFieldValue<Inst, Field_qqqqqq>(op) + 0xffff80;

		if (m_opWordB == 0 && addr == Esai::M_SAISR && bit == Esai::M_TFS)
		{
			// op word B = jump to self, addr = ESAI status register, bit test for bit Transmit Frame Sync
			DspValue count(m_block);

			// Use our custom register to read the remaining number of instructions until frame sync flips
			m_block.mem().readPeriph(count, MemArea_X, BitValue == BitSet ? Esai::RemainingInstructionsForFrameSyncFalse : Esai::RemainingInstructionsForFrameSyncTrue, Inst);

			If(m_block, m_blockRuntimeData, [&](auto _toFalse)
			{
				// do nothing if the value is zero
				m_asm.test_(r32(count.get()));
				m_asm.jz(_toFalse);
			}, [&]()
			{
				// If the value is positive, increase the number of executed instructions to make ESAI do the frame sync
				m_block.increaseInstructionCount(r32(count.get()));
			}, false);
		}
	}

	template<Instruction Inst, BraMode Bmode> void JitOps::braIfCC(const TWord op, DspValue& offset)
	{
		DSPReg pc(m_block, JitDspRegPool::DspPC, true, true);

		checkCondition<Inst>(op, [&]()
		{
			braOrBsr<Bmode>(offset);
		}, Bmode == Bsr);
	}

	// ________________________________________________
	// JMP (absolute jumps)

	template <> inline void JitOps::jumpOrJSR<Jump>(DspValue& _ea)			{ jmp(_ea); }
	template <> inline void JitOps::jumpOrJSR<JSR>(DspValue& _ea)			{ jsr(_ea); }

	template<Instruction Inst, JumpMode Bmode> void JitOps::jumpIfCC(const TWord op, DspValue& offset)
	{
		DSPReg pc(m_block, JitDspRegPool::DspPC, true, true);

		checkCondition<Inst>(op, [&]()
		{
			jumpOrJSR<Bmode>(offset);
		}, Bmode == JSR);
	}

	template<Instruction Inst, JumpMode Bmode, ExpectedBitValue BitValue> void JitOps::jumpIfBitTestMem(const TWord _op)
	{
		const auto addr = absAddressExt<Inst>();
		DspValue a(m_block, addr, DspValue::Immediate24);

		DSPReg pc(m_block, JitDspRegPool::DspPC, true, true);
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

		DSPReg pc(m_block, JitDspRegPool::DspPC, true, true);
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
