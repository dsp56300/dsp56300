#include "jitops.h"

#include "jitops_jmp.inl"
#include "dsp.h"

namespace dsp56k
{
	TWord callDspRemainingInstructionsForFrameSyncFalse(DSP* _dsp)
	{
		auto* p = static_cast<Peripherals56362*>(_dsp->getPeriph(0));  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		return p->getEsaiClock().getRemainingInstructionsForFrameSync<false>();
	}

	TWord callDspRemainingInstructionsForFrameSyncTrue(DSP* _dsp)
	{
		auto* p = static_cast<Peripherals56362*>(_dsp->getPeriph(0));  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		return p->getEsaiClock().getRemainingInstructionsForFrameSync<true>();
	}

	template<Instruction Inst, ExpectedBitValue BitValue> void JitOps::esaiFrameSyncSpinloop(const TWord op) const
	{
		if(!dynamic_cast<Peripherals56362*>(getBlock().dsp().getPeriph(0)))
			return;

		// If the DSP is spinlooping while waiting for the ESAI frame sync to flip, we fast-forward the DSP instructions to make it happen ASAP
		const auto bit = getBit<Inst>(op);
		const auto addr = getFieldValue<Inst, Field_qqqqqq>(op) + 0xffff80;

		if (m_opWordB == 0 && addr == Esai::M_SAISR && bit == Esai::M_TFS)
		{
			{
				const FuncArg r0(m_block, 0);
				m_block.mem().makeDspPtr(r0);
				m_block.stack().call(asmjit::func_as_ptr(BitValue == BitSet ? &callDspRemainingInstructionsForFrameSyncFalse : &callDspRemainingInstructionsForFrameSyncTrue));
			}

			// op word B = jump to self, addr = ESAI status register, bit test for bit Transmit Frame Sync
			auto count = r32(regReturnVal);

			const SkipLabel skip(m_block);
			// do nothing if the value is zero
			m_asm.test_(count);
			m_asm.jz(skip);

			// If the value is positive, increase the number of executed instructions to make ESAI do the frame sync

#ifdef HAVE_ARM64
			// increaseInstructionCount uses the scratch reg on ARM, which is identical to the return value reg. We need to move our count elsewhere
			const RegGP temp(m_block);
			m_asm.mov(r32(temp), r32(count));
			count = r32(temp);
#endif

			m_block.increaseInstructionCount(r32(count));

			// TODO: we should increase by the cycle count here, but as the custom peripheral func still works on instructions, we need to use 1 for now
//			const auto cycles = dsp56k::calcCycles(Inst, m_pcCurrentOp, op, m_block.dsp().memory().getBridgedMemoryAddress(), 1);
//			m_asm.imul(r32(count), asmjit::Imm(cycles));
			m_block.increaseCycleCount(r32(count));
		}
	}

	// Brclr
	void JitOps::op_Brclr_ea(const TWord op) { braIfBitTestMem<Brclr_ea, Bra, BitClear>(op); }
	void JitOps::op_Brclr_aa(const TWord op) { braIfBitTestMem<Brclr_aa, Bra, BitClear>(op); }
	void JitOps::op_Brclr_pp(const TWord op) { braIfBitTestMem<Brclr_pp, Bra, BitClear>(op); }
	void JitOps::op_Brclr_qq(const TWord op)
	{
		esaiFrameSyncSpinloop<Brclr_qq, BitClear>(op);
		braIfBitTestMem<Brclr_qq, Bra, BitClear>(op);
	}
	void JitOps::op_Brclr_S(const TWord op) { braIfBitTestDDDDDD<Brclr_S, Bra, BitClear>(op); }

	// Brset
	void JitOps::op_Brset_ea(const TWord op) { braIfBitTestMem<Brset_ea, Bra, BitSet>(op); }
	void JitOps::op_Brset_aa(const TWord op) { braIfBitTestMem<Brset_aa, Bra, BitSet>(op); }
	void JitOps::op_Brset_pp(const TWord op) { braIfBitTestMem<Brset_pp, Bra, BitSet>(op); }
	void JitOps::op_Brset_qq(const TWord op)
	{
		esaiFrameSyncSpinloop<Brset_qq, BitSet>(op);
		braIfBitTestMem<Brset_qq, Bra, BitSet>(op);
	}
	void JitOps::op_Brset_S(const TWord op) { braIfBitTestDDDDDD<Brset_S, Bra, BitSet>(op); }

	// Bsclr
	void JitOps::op_Bsclr_ea(const TWord op) { braIfBitTestMem<Brclr_ea, Bsr, BitClear>(op); }
	void JitOps::op_Bsclr_aa(const TWord op) { braIfBitTestMem<Brclr_aa, Bsr, BitClear>(op); }
	void JitOps::op_Bsclr_pp(const TWord op) { braIfBitTestMem<Brclr_pp, Bsr, BitClear>(op); }
	void JitOps::op_Bsclr_qq(const TWord op) { braIfBitTestMem<Brclr_qq, Bsr, BitClear>(op); }
	void JitOps::op_Bsclr_S(const TWord op) { braIfBitTestDDDDDD<Brclr_S, Bsr, BitClear>(op); }

	// Bsset
	void JitOps::op_Bsset_ea(const TWord op) { braIfBitTestMem<Bsset_ea, Bsr, BitSet>(op); }
	void JitOps::op_Bsset_aa(const TWord op) { braIfBitTestMem<Bsset_aa, Bsr, BitSet>(op); }
	void JitOps::op_Bsset_pp(const TWord op) { braIfBitTestMem<Bsset_pp, Bsr, BitSet>(op); }
	void JitOps::op_Bsset_qq(const TWord op) { braIfBitTestMem<Bsset_qq, Bsr, BitSet>(op); }
	void JitOps::op_Bsset_S(const TWord op) { braIfBitTestDDDDDD<Bsset_S, Bsr, BitSet>(op); }

	void JitOps::op_Bra_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bra_xxxx>();
		DspValue o(m_block, offset, DspValue::Immediate24);
		braOrBsr<Bra>(o);
	}

	void JitOps::op_Bra_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<Bra_xxx>(op);
		DspValue o(m_block, offset, DspValue::Immediate24);
		braOrBsr<Bra>(o);
	}

	void JitOps::op_Bra_Rn(TWord op)
	{
		const auto rrr = getFieldValue<Bra_Rn, Field_RRR>(op);

		auto r = m_dspRegs.getR(rrr);
		r.toTemp();
		braOrBsr<Bra>(r);
	}

	void JitOps::op_Bsr_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bsr_xxxx>();
		DspValue o(m_block, offset, DspValue::Immediate24);
		braOrBsr<Bsr>(o);
	}

	void JitOps::op_Bsr_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<Bsr_xxx>(op);
		DspValue o(m_block, offset, DspValue::Immediate24);
		braOrBsr<Bsr>(o);
	}

	void JitOps::op_Bsr_Rn(TWord op)
	{
		const auto rrr = getFieldValue<Bsr_Rn, Field_RRR>(op);

		auto r = m_dspRegs.getR(rrr);
		r.toTemp();
		braOrBsr<Bsr>(r);
	}

	void JitOps::op_Bcc_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bcc_xxxx>();
		DspValue o(m_block, offset, DspValue::Immediate24);
		braIfCC<Bcc_xxxx, Bra>(op, o);
	}

	void JitOps::op_Bcc_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<Bcc_xxx>(op);
		DspValue o(m_block, offset, DspValue::Immediate24);
		braIfCC<Bcc_xxx, Bra>(op, o);
	}

	void JitOps::op_Bcc_Rn(TWord op)
	{
		const auto rrr = getFieldValue<Bcc_Rn, Field_RRR>(op);

		auto r = m_dspRegs.getR(rrr);
		r.toTemp();
		braIfCC<Bcc_Rn, Bra>(op, r);
	}

	void JitOps::op_BScc_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<BScc_xxxx>();
		DspValue o(m_block, offset, DspValue::Immediate24);
		braIfCC<Bcc_xxxx, Bsr>(op, o);
	}

	void JitOps::op_BScc_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<BScc_xxx>(op);
		DspValue o(m_block, offset, DspValue::Immediate24);
		braIfCC<Bcc_xxx, Bsr>(op, o);
	}

	void JitOps::op_BScc_Rn(TWord op)
	{
		const auto rrr = getFieldValue<BScc_Rn, Field_RRR>(op);

		auto r = m_dspRegs.getR(rrr);
		r.toTemp();
		braIfCC<Bcc_Rn, Bsr>(op, r);
	}

	void JitOps::op_Jclr_ea(const TWord op) { jumpIfBitTestMem<Jclr_ea, Jump, BitClear>(op); }
	void JitOps::op_Jclr_aa(const TWord op) { jumpIfBitTestMem<Jclr_aa, Jump, BitClear>(op); }
	void JitOps::op_Jclr_pp(const TWord op) { jumpIfBitTestMem<Jclr_pp, Jump, BitClear>(op); }
	void JitOps::op_Jclr_qq(const TWord op) { jumpIfBitTestMem<Jclr_qq, Jump, BitClear>(op); }
	void JitOps::op_Jclr_S(const TWord op) { jumpIfBitTestDDDDDD<Jclr_S, Jump, BitClear>(op); }

	void JitOps::op_Jsclr_ea(const TWord op) { jumpIfBitTestMem<Jsclr_ea, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_aa(const TWord op) { jumpIfBitTestMem<Jsclr_aa, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_pp(const TWord op) { jumpIfBitTestMem<Jsclr_pp, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_qq(const TWord op) { jumpIfBitTestMem<Jsclr_qq, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_S(const TWord op) { jumpIfBitTestDDDDDD<Jsclr_S, JSR, BitClear>(op); }
	void JitOps::op_Jset_ea(const TWord op) { jumpIfBitTestMem<Jset_ea, Jump, BitSet>(op); }
	void JitOps::op_Jset_aa(const TWord op) { jumpIfBitTestMem<Jset_aa, Jump, BitSet>(op); }
	void JitOps::op_Jset_pp(const TWord op) { jumpIfBitTestMem<Jset_pp, Jump, BitSet>(op); }
	void JitOps::op_Jset_qq(const TWord op) { jumpIfBitTestMem<Jset_qq, Jump, BitSet>(op); }
	void JitOps::op_Jset_S(const TWord op) { jumpIfBitTestDDDDDD<Jset_S, Jump, BitSet>(op); }

	void JitOps::op_Jsset_ea(const TWord op) { jumpIfBitTestMem<Jsset_ea, JSR, BitSet>(op); }
	void JitOps::op_Jsset_aa(const TWord op) { jumpIfBitTestMem<Jsset_aa, JSR, BitSet>(op); }
	void JitOps::op_Jsset_pp(const TWord op) { jumpIfBitTestMem<Jsset_pp, JSR, BitSet>(op); }
	void JitOps::op_Jsset_qq(const TWord op) { jumpIfBitTestMem<Jsset_qq, JSR, BitSet>(op); }
	void JitOps::op_Jsset_S(const TWord op) { jumpIfBitTestDDDDDD<Jsset_S, JSR, BitSet>(op); }

	void JitOps::op_Jcc_ea(TWord op)
	{
		auto ea = effectiveAddress<Jcc_ea>(op);
		jumpIfCC<Jcc_ea, Jump>(op, ea);
	}

	void JitOps::op_Jcc_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jcc_xxx, Field_aaaaaaaaaaaa>(op);
		DspValue a(m_block, addr, DspValue::Immediate24);
		jumpIfCC<Jcc_xxx, Jump>(op, a);
	}

	void JitOps::op_Jmp_ea(TWord op)
	{
		auto ea = effectiveAddress<Jmp_ea>(op);
		jumpOrJSR<Jump>(ea);
	}

	void JitOps::op_Jmp_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jmp_xxx, Field_aaaaaaaaaaaa>(op);
		DspValue a(m_block, addr, DspValue::Immediate24);
		jumpOrJSR<Jump>(a);
	}

	void JitOps::op_Jscc_ea(TWord op)
	{
		auto ea = effectiveAddress<Jscc_ea>(op);
		jumpIfCC<Jscc_ea, JSR>(op, ea);
	}

	void JitOps::op_Jscc_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jscc_xxx, Field_aaaaaaaaaaaa>(op);
		DspValue a(m_block, addr, DspValue::Immediate24);
		jumpIfCC<Jscc_xxx, JSR>(op, a);
	}

	void JitOps::op_Jsr_ea(TWord op)
	{
		auto ea = effectiveAddress<Jsr_ea>(op);
		jumpOrJSR<JSR>(ea);
	}

	void JitOps::op_Jsr_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jsr_xxx, Field_aaaaaaaaaaaa>(op);
		DspValue a(m_block, addr, DspValue::Immediate24);
		jumpOrJSR<JSR>(a);
	}
}
