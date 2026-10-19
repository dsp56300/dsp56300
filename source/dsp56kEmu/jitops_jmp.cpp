#include "jitops.h"

#include "jitops_jmp.inl"
#include "dsp.h"

namespace dsp56k
{
	void skipToFrameSync(DSP* _dsp, TWord _instructions)
	{
//		m_block.increaseInstructionCount(r32(count));
		// TODO: we should increase by the cycle count here, but as the custom peripheral func still works on instructions, we need to use 1 for now
//		const auto cycles = dsp56k::calcCycles(Inst, m_pcCurrentOp, op, m_block.dsp().memory().getBridgedMemoryAddress(), 1);
//		m_asm.imul(r32(count), asmjit::Imm(cycles));
//		m_block.increaseCycleCount(r32(count));

		_dsp->fastForward(_instructions, _instructions);
	}

	template<bool ExpectedValue, TWord Bit>
	void callDspSkipToEsaiSlot(DSP* _dsp)
	{
		auto* p = static_cast<Peripherals56362*>(_dsp->getPeriph(0));  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		const bool flag = bittest<TWord, Bit>(p->getEsai().readStatusRegister());
		skipToFrameSync(_dsp, p->getEsaiClock().getRemainingInstructionsUntil<ExpectedValue>(flag));
	}

	template<bool ExpectedValue, uint32_t EssiIndex, TWord Bit>
	void callDspSkipToEssiSlot(DSP* _dsp)
	{
		auto* p = static_cast<Peripherals56303*>(_dsp->getPeriph(0));  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		const bool flag = bittest<TWord, Bit>((EssiIndex ? p->getEssi1() : p->getEssi0()).readSR());
		skipToFrameSync(_dsp, p->getEssiClock().getRemainingInstructionsUntil<ExpectedValue>(flag));
	}

	using SkipToSlotFunc = void (*)(DSP*);

	template<bool ExpectedValue> SkipToSlotFunc esaiSkipToSlot(const TWord _bit)
	{
		switch(_bit)
		{
		case Esai::M_TFS:	return &callDspSkipToEsaiSlot<ExpectedValue, Esai::M_TFS>;
		case Esai::M_RFS:	return &callDspSkipToEsaiSlot<ExpectedValue, Esai::M_RFS>;
		case Esai::M_TDE:	return &callDspSkipToEsaiSlot<ExpectedValue, Esai::M_TDE>;
		case Esai::M_RDF:	return &callDspSkipToEsaiSlot<ExpectedValue, Esai::M_RDF>;
		default:			return nullptr;
		}
	}

	template<bool ExpectedValue, uint32_t EssiIndex> SkipToSlotFunc essiSkipToSlot(const TWord _bit)
	{
		switch(_bit)
		{
		case Essi::SSISR_TFS:	return &callDspSkipToEssiSlot<ExpectedValue, EssiIndex, Essi::SSISR_TFS>;
		case Essi::SSISR_RFS:	return &callDspSkipToEssiSlot<ExpectedValue, EssiIndex, Essi::SSISR_RFS>;
		case Essi::SSISR_TDE:	return &callDspSkipToEssiSlot<ExpectedValue, EssiIndex, Essi::SSISR_TDE>;
		case Essi::SSISR_RDF:	return &callDspSkipToEssiSlot<ExpectedValue, EssiIndex, Essi::SSISR_RDF>;
		default:				return nullptr;
		}
	}

	/*	A DSP that spins on a flag of its serial interface - a frame sync, a transmit register that became empty or a
		receive register that became full - waits for the next slot, so fast forward it to that slot. Spinning, it runs
		several cycles per instruction, many more than the code the clock measured its ratio on: after a restart of the
		clock, a poll for the first received word ran past the slot by more than a slot and found an overrun
	*/
	template<Instruction Inst, ExpectedBitValue BitValue> void JitOps::serialStatusSpinloop(const TWord op, const bool _loopsToItself) const
	{
		if(!_loopsToItself)
			return;

		// spinning while the bit is clear waits for it to be set
		serialStatusSpin(getFieldValue<Inst, Field_qqqqqq>(op) + 0xffff80, getBit<Inst>(op), BitValue == BitClear);
	}

	void JitOps::serialStatusSpin(const TWord _addr, const TWord _bit, const bool _waitForSet) const
	{
		SkipToSlotFunc func = nullptr;

		if(dynamic_cast<Peripherals56362*>(getBlock().dsp().getPeriph(0)))
		{
			if(_addr == Esai::M_SAISR)
				func = _waitForSet ? esaiSkipToSlot<true>(_bit) : esaiSkipToSlot<false>(_bit);
		}
		else if(dynamic_cast<Peripherals56303*>(getBlock().dsp().getPeriph(0)))
		{
			if(_addr == Essi::ESSI0_SSISR)
				func = _waitForSet ? essiSkipToSlot<true, 0>(_bit) : essiSkipToSlot<false, 0>(_bit);
			else if(_addr == Essi::ESSI1_SSISR)
				func = _waitForSet ? essiSkipToSlot<true, 1>(_bit) : essiSkipToSlot<false, 1>(_bit);
		}

		if(!func)
			return;

		const FuncArg r0(m_block, 0);
		m_block.mem().makeDspPtr(r0);
		m_block.stack().call(asmjit::func_as_ptr(func));
	}

	/*	The same spin in two instructions: a btst of the status register, and a branch on carry back to it. The Nord Lead
		2X waits for the transmit frame sync with btst #TFS,x:>$ffb3 / jcc, then arms the DMA channel that feeds its slots
		1, 2, 3 and 0. It has to be armed before slot 1 starts. Spinning, the clock reached slot 0 late, served slot 1 an
		instruction after it to catch up, and the stream went out a slot late until the next miss shifted it back
	*/
	template<Instruction Inst> void JitOps::btstSerialStatusSpinloop(const TWord op) const
	{
		if constexpr (Inst == Btst_qq)
		{
			btstSerialStatusSpin(getFieldValueMemArea<Btst_qq>(op), getFieldValue<Btst_qq, Field_qqqqqq>(op) + 0xffff80, getBit<Btst_qq>(op));
		}
		else
		{
			static_assert(Inst == Btst_ea);

			// an absolute address is known while compiling, any other is not
			if((getFieldValue<Btst_ea, Field_MMM>(op) << 3 | getFieldValue<Btst_ea, Field_RRR>(op)) == MMMRRR_AbsAddr)
				btstSerialStatusSpin(getFieldValueMemArea<Btst_ea>(op), m_opWordB, getBit<Btst_ea>(op));
		}
	}

	template void JitOps::btstSerialStatusSpinloop<Btst_qq>(TWord) const;
	template void JitOps::btstSerialStatusSpinloop<Btst_ea>(TWord) const;

	void JitOps::btstSerialStatusSpin(const EMemArea _area, TWord _addr, const TWord _bit) const
	{
		if(_area != MemArea_X)
			return;

		// a 16 bit address of the peripheral range, in Sixteen-bit Compatibility mode
		if(_addr < 0x10000 && isPeriphAddress(_addr))
			_addr |= 0xff0000;

		const auto& mem = m_block.dsp().memory();

		const auto pc = m_pcCurrentOp + m_opcodes.getOpcodeLength(m_opWordA);
		const auto op = mem.get(MemArea_P, pc);

		Instruction inst, instB;
		m_opcodes.getInstructionTypes(op, inst, instB);

		TWord target;
		TWord cccc;

		switch(inst)
		{
		case Jcc_xxx:
			target = getFieldValue<Jcc_xxx, Field_aaaaaaaaaaaa>(op);
			cccc = getFieldValue<Jcc_xxx, Field_CCCC>(op);
			break;
		case Jcc_ea:
			if((getFieldValue<Jcc_ea, Field_MMM>(op) << 3 | getFieldValue<Jcc_ea, Field_RRR>(op)) != MMMRRR_AbsAddr)
				return;
			target = mem.get(MemArea_P, pc + 1);
			cccc = getFieldValue<Jcc_ea, Field_CCCC>(op);
			break;
		case Bcc_xxx:
			target = pc + getRelativeAddressOffset<Bcc_xxx>(op);
			cccc = getFieldValue<Bcc_xxx, Field_CCCC>(op);
			break;
		case Bcc_xxxx:
			target = pc + signextend<int, 24>(static_cast<int>(mem.get(MemArea_P, pc + 1)));
			cccc = getFieldValue<Bcc_xxxx, Field_CCCC>(op);
			break;
		default:
			return;
		}

		if((target & 0xffffff) != m_pcCurrentOp)
			return;

		// btst copies the bit to the carry: looping while it is clear waits for the bit to be set
		if(cccc == CCCC_CarryClear)
			serialStatusSpin(_addr, _bit, true);
		else if(cccc == CCCC_CarrySet)
			serialStatusSpin(_addr, _bit, false);
	}

	// Brclr
	void JitOps::op_Brclr_ea(const TWord op) { braIfBitTestMem<Brclr_ea, Bra, BitClear>(op); }
	void JitOps::op_Brclr_aa(const TWord op) { braIfBitTestMem<Brclr_aa, Bra, BitClear>(op); }
	void JitOps::op_Brclr_pp(const TWord op) { braIfBitTestMem<Brclr_pp, Bra, BitClear>(op); }
	void JitOps::op_Brclr_qq(const TWord op)
	{
		serialStatusSpinloop<Brclr_qq, BitClear>(op, m_opWordB == 0);
		braIfBitTestMem<Brclr_qq, Bra, BitClear>(op);
	}
	void JitOps::op_Brclr_S(const TWord op) { braIfBitTestDDDDDD<Brclr_S, Bra, BitClear>(op); }

	// Brset
	void JitOps::op_Brset_ea(const TWord op) { braIfBitTestMem<Brset_ea, Bra, BitSet>(op); }
	void JitOps::op_Brset_aa(const TWord op) { braIfBitTestMem<Brset_aa, Bra, BitSet>(op); }
	void JitOps::op_Brset_pp(const TWord op) { braIfBitTestMem<Brset_pp, Bra, BitSet>(op); }
	void JitOps::op_Brset_qq(const TWord op)
	{
		serialStatusSpinloop<Brset_qq, BitSet>(op, m_opWordB == 0);
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
	void JitOps::op_Jclr_qq(const TWord op)
	{
		serialStatusSpinloop<Jclr_qq, BitClear>(op, m_opWordB == m_pcCurrentOp);
		jumpIfBitTestMem<Jclr_qq, Jump, BitClear>(op);
	}
	void JitOps::op_Jclr_S(const TWord op) { jumpIfBitTestDDDDDD<Jclr_S, Jump, BitClear>(op); }

	void JitOps::op_Jsclr_ea(const TWord op) { jumpIfBitTestMem<Jsclr_ea, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_aa(const TWord op) { jumpIfBitTestMem<Jsclr_aa, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_pp(const TWord op) { jumpIfBitTestMem<Jsclr_pp, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_qq(const TWord op) { jumpIfBitTestMem<Jsclr_qq, JSR, BitClear>(op); }
	void JitOps::op_Jsclr_S(const TWord op) { jumpIfBitTestDDDDDD<Jsclr_S, JSR, BitClear>(op); }
	void JitOps::op_Jset_ea(const TWord op) { jumpIfBitTestMem<Jset_ea, Jump, BitSet>(op); }
	void JitOps::op_Jset_aa(const TWord op) { jumpIfBitTestMem<Jset_aa, Jump, BitSet>(op); }
	void JitOps::op_Jset_pp(const TWord op) { jumpIfBitTestMem<Jset_pp, Jump, BitSet>(op); }
	void JitOps::op_Jset_qq(const TWord op)
	{
		serialStatusSpinloop<Jclr_qq, BitSet>(op, m_opWordB == m_pcCurrentOp);
		jumpIfBitTestMem<Jset_qq, Jump, BitSet>(op);
	}
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
