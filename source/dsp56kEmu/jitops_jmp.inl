#pragma once

#include "jitops.h"

namespace dsp56k
{
	// ________________________________________________
	// BRA (relative jumps)

	template <> inline void JitOps::braOrBsr<Bra>(int _offset)	{ jmp(m_pcCurrentOp + _offset); }
	template <> inline void JitOps::braOrBsr<Bsr>(int _offset)	{ jsr(m_pcCurrentOp + _offset); }

	template <> inline void JitOps::braOrBsr<Bra>(const JitReg32& _offset)	{ m_asm.add(_offset, asmjit::Imm(m_pcCurrentOp)); jmp(_offset); }
	template <> inline void JitOps::braOrBsr<Bsr>(const JitReg32& _offset)	{ m_asm.add(_offset, asmjit::Imm(m_pcCurrentOp)); jsr(_offset); }

	template<Instruction Inst, BraMode BMode, ExpectedBitValue BitValue> void JitOps::braIfBitTestMem(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		const auto end = m_asm.newLabel();

		m_dspRegs.notifyBeginBranch();

		bitTestMemory<Inst>(op, BitValue, end);

		braOrBsr<BMode>(addr);

		m_asm.bind(end);
	}

	template<Instruction Inst, BraMode BMode, ExpectedBitValue BitValue> void JitOps::braIfBitTestDDDDDD(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		const auto end = m_asm.newLabel();

		const auto dddddd = getFieldValue<Inst,Field_DDDDDD>(op);

		const RegGP r(m_block);
		decode_dddddd_read(r.get().r32(), dddddd);

		m_dspRegs.notifyBeginBranch();

		bitTest<Inst>(op, r, BitValue, end);

		braOrBsr<BMode>(addr);

		m_asm.bind(end);
	}

	// Brclr
	inline void JitOps::op_Brclr_ea(const TWord op)	{ braIfBitTestMem<Brclr_ea, Bra, BitClear>(op);	}
	inline void JitOps::op_Brclr_aa(const TWord op)	{ braIfBitTestMem<Brclr_aa, Bra, BitClear>(op);	}
	inline void JitOps::op_Brclr_pp(const TWord op)	{ braIfBitTestMem<Brclr_pp, Bra, BitClear>(op);	}
	inline void JitOps::op_Brclr_qq(const TWord op)	{ braIfBitTestMem<Brclr_qq, Bra, BitClear>(op);	}
	inline void JitOps::op_Brclr_S(const TWord op)	{ braIfBitTestDDDDDD<Brclr_S, Bra, BitClear>(op); }

	// Brset
	inline void JitOps::op_Brset_ea(const TWord op)	{ braIfBitTestMem<Brset_ea, Bra, BitSet>(op); }
	inline void JitOps::op_Brset_aa(const TWord op)	{ braIfBitTestMem<Brset_aa, Bra, BitSet>(op); }
	inline void JitOps::op_Brset_pp(const TWord op)	{ braIfBitTestMem<Brset_pp, Bra, BitSet>(op); }
	inline void JitOps::op_Brset_qq(const TWord op)	{ braIfBitTestMem<Brset_qq, Bra, BitSet>(op); }
	inline void JitOps::op_Brset_S(const TWord op)	{ braIfBitTestDDDDDD<Brset_S, Bra, BitSet>(op); }

	// Bsclr
	inline void JitOps::op_Bsclr_ea(const TWord op)	{ braIfBitTestMem<Brclr_ea, Bsr, BitClear>(op);	}
	inline void JitOps::op_Bsclr_aa(const TWord op)	{ braIfBitTestMem<Brclr_aa, Bsr, BitClear>(op);	}
	inline void JitOps::op_Bsclr_pp(const TWord op)	{ braIfBitTestMem<Brclr_pp, Bsr, BitClear>(op);	}
	inline void JitOps::op_Bsclr_qq(const TWord op)	{ braIfBitTestMem<Brclr_qq, Bsr, BitClear>(op);	}
	inline void JitOps::op_Bsclr_S(const TWord op)	{ braIfBitTestDDDDDD<Brclr_S, Bsr, BitClear>(op); }

	// Bsset
	inline void JitOps::op_Bsset_ea(const TWord op)	{ braIfBitTestMem<Bsset_ea, Bsr, BitSet>(op); }
	inline void JitOps::op_Bsset_aa(const TWord op)	{ braIfBitTestMem<Bsset_aa, Bsr, BitSet>(op); }
	inline void JitOps::op_Bsset_pp(const TWord op)	{ braIfBitTestMem<Bsset_pp, Bsr, BitSet>(op); }
	inline void JitOps::op_Bsset_qq(const TWord op)	{ braIfBitTestMem<Bsset_qq, Bsr, BitSet>(op); }
	inline void JitOps::op_Bsset_S(const TWord op)	{ braIfBitTestDDDDDD<Bsset_S, Bsr, BitSet>(op); }	

	template<Instruction Inst, BraMode Bmode, typename TOff> void JitOps::braIfCC(const TWord op, const TOff& offset)
	{
		const auto end = m_asm.newLabel();

		m_dspRegs.notifyBeginBranch();

		checkCondition<Inst>(op);

		m_asm.jz(end);

		braOrBsr<Bmode>(offset);

		m_asm.bind(end);
	}

	inline void JitOps::op_Bra_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bra_xxxx>();
		braOrBsr<Bra>(offset);
	}

	inline void JitOps::op_Bra_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<Bra_xxx>(op);
		braOrBsr<Bra>(offset);
	}

	inline void JitOps::op_Bra_Rn(TWord op)
	{
		const auto rrr = getFieldValue<Bra_Rn, Field_RRR>(op);

		const RegGP r(m_block);
		m_dspRegs.getR(r, rrr);

		braOrBsr<Bra>(r.get().r32());
	}

	inline void JitOps::op_Bsr_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bsr_xxxx>();
		braOrBsr<Bsr>(offset);
	}

	inline void JitOps::op_Bsr_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<Bsr_xxx>(op);
		braOrBsr<Bsr>(offset);
	}

	inline void JitOps::op_Bsr_Rn(TWord op)
	{
		const auto rrr = getFieldValue<Bsr_Rn, Field_RRR>(op);

		const RegGP r(m_block);
		m_dspRegs.getR(r, rrr);

		braOrBsr<Bsr>(r.get().r32());
	}

	inline void JitOps::op_Bcc_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<Bcc_xxxx>();
		braIfCC<Bcc_xxxx, Bra>(op, offset);
	}

	inline void JitOps::op_Bcc_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<Bcc_xxx>(op);
		braIfCC<Bcc_xxx, Bra>(op, offset);
	}

	inline void JitOps::op_Bcc_Rn(TWord op)
	{
		const auto rrr = getFieldValue<Bcc_Rn, Field_RRR>(op);

		const RegGP r(m_block);
		m_dspRegs.getR(r, rrr);

		braIfCC<Bcc_Rn, Bra>(op, r.get().r32());
	}

	inline void JitOps::op_BScc_xxxx(TWord op)
	{
		const auto offset = pcRelativeAddressExt<BScc_xxxx>();
		braIfCC<Bcc_xxxx, Bsr>(op, offset);
	}

	inline void JitOps::op_BScc_xxx(TWord op)
	{
		const auto offset = getRelativeAddressOffset<BScc_xxx>(op);
		braIfCC<Bcc_xxx, Bsr>(op, offset);
	}

	inline void JitOps::op_BScc_Rn(TWord op)
	{
		const auto rrr = getFieldValue<BScc_Rn, Field_RRR>(op);

		const RegGP r(m_block);
		m_dspRegs.getR(r, rrr);

		braIfCC<Bcc_Rn, Bsr>(op, r.get().r32());
	}

	// ________________________________________________
	// JMP (absolute jumps)

	template <> inline void JitOps::jumpOrJSR<Jump>(const TWord _ea)		{ jmp(_ea); }
	template <> inline void JitOps::jumpOrJSR<JSR>(const TWord _ea)			{ jsr(_ea); }

	template <> inline void JitOps::jumpOrJSR<Jump>(const JitReg32& _ea)	{ jmp(_ea); }
	template <> inline void JitOps::jumpOrJSR<JSR>(const JitReg32& _ea)		{ jsr(_ea); }
	
	template<Instruction Inst, JumpMode Bmode, typename TAbsAddr> void JitOps::jumpIfCC(const TWord op, const TAbsAddr& offset)
	{
		const auto end = m_asm.newLabel();

		m_dspRegs.notifyBeginBranch();

		checkCondition<Inst>(op);

		m_asm.jz(end);

		jumpOrJSR<Bmode>(offset);

		m_asm.bind(end);
	}

	template<Instruction Inst, JumpMode Jsr, ExpectedBitValue BitValue> void JitOps::jumpIfBitTestMem(const TWord _op)
	{
		const auto addr = absAddressExt<Inst>();

		const auto end = m_asm.newLabel();

		m_dspRegs.notifyBeginBranch();

		bitTestMemory<Inst>(_op, BitValue, end);

		jumpOrJSR<Jsr>(addr);

		m_asm.bind(end);
	}

	template<Instruction Inst, JumpMode Jsr, ExpectedBitValue BitValue> void JitOps::jumpIfBitTestDDDDDD(const TWord op)
	{
		const auto addr = absAddressExt<Inst>();

		const auto end = m_asm.newLabel();

		const auto dddddd = getFieldValue<Inst,Field_DDDDDD>(op);

		const RegGP r(m_block);
		decode_dddddd_read(r.get().r32(), dddddd);

		m_dspRegs.notifyBeginBranch();

		bitTest<Inst>(op, r, BitValue, end);

		jumpOrJSR<Jsr>(addr);

		m_asm.bind(end);
	}

	inline void JitOps::op_Jclr_ea(const TWord op)		{ jumpIfBitTestMem<Jclr_ea, Jump, BitClear>(op); }
	inline void JitOps::op_Jclr_aa(const TWord op)		{ jumpIfBitTestMem<Jclr_aa, Jump, BitClear>(op); }
	inline void JitOps::op_Jclr_pp(const TWord op)		{ jumpIfBitTestMem<Jclr_pp, Jump, BitClear>(op); }
	inline void JitOps::op_Jclr_qq(const TWord op)		{ jumpIfBitTestMem<Jclr_qq, Jump, BitClear>(op); }
	inline void JitOps::op_Jclr_S(const TWord op)		{ jumpIfBitTestDDDDDD<Jclr_S, Jump, BitClear>(op); }

	inline void JitOps::op_Jsclr_ea(const TWord op)		{ jumpIfBitTestMem<Jsclr_ea, JSR, BitClear>(op); }
	inline void JitOps::op_Jsclr_aa(const TWord op)		{ jumpIfBitTestMem<Jsclr_aa, JSR, BitClear>(op); }
	inline void JitOps::op_Jsclr_pp(const TWord op)		{ jumpIfBitTestMem<Jsclr_pp, JSR, BitClear>(op); }
	inline void JitOps::op_Jsclr_qq(const TWord op)		{ jumpIfBitTestMem<Jsclr_qq, JSR, BitClear>(op); }
	inline void JitOps::op_Jsclr_S(const TWord op)		{ jumpIfBitTestDDDDDD<Jsclr_S, JSR, BitClear>(op); }
	inline void JitOps::op_Jset_ea(const TWord op)		{ jumpIfBitTestMem<Jset_ea, Jump, BitSet>(op); }
	inline void JitOps::op_Jset_aa(const TWord op)		{ jumpIfBitTestMem<Jset_aa, Jump, BitSet>(op); }
	inline void JitOps::op_Jset_pp(const TWord op)		{ jumpIfBitTestMem<Jset_pp, Jump, BitSet>(op); }
	inline void JitOps::op_Jset_qq(const TWord op)		{ jumpIfBitTestMem<Jset_qq, Jump, BitSet>(op); }
	inline void JitOps::op_Jset_S(const TWord op)		{ jumpIfBitTestDDDDDD<Jset_S, Jump, BitSet>(op); }

	inline void JitOps::op_Jsset_ea(const TWord op)		{ jumpIfBitTestMem<Jsset_ea, JSR, BitSet>(op); }
	inline void JitOps::op_Jsset_aa(const TWord op)		{ jumpIfBitTestMem<Jsset_aa, JSR, BitSet>(op); }
	inline void JitOps::op_Jsset_pp(const TWord op)		{ jumpIfBitTestMem<Jsset_pp, JSR, BitSet>(op); }
	inline void JitOps::op_Jsset_qq(const TWord op)		{ jumpIfBitTestMem<Jsset_qq, JSR, BitSet>(op); }
	inline void JitOps::op_Jsset_S(const TWord op)		{ jumpIfBitTestDDDDDD<Jsset_S, JSR, BitSet>(op); }

	inline void JitOps::op_Jcc_ea(TWord op)
	{
		RegGP ea(m_block);
		effectiveAddress<Jcc_ea>(ea, op);
		jumpIfCC<Jcc_ea, Jump>(op, ea.get().r32());
	}

	inline void JitOps::op_Jcc_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jcc_xxx, Field_aaaaaaaaaaaa>(op);
		jumpIfCC<Jcc_xxx, Jump>(op, addr);
	}

	inline void JitOps::op_Jmp_ea(TWord op)
	{
		RegGP ea(m_block);
		effectiveAddress<Jmp_ea>(ea, op);
		jumpOrJSR<Jump>(ea.get().r32());
	}

	inline void JitOps::op_Jmp_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jmp_xxx, Field_aaaaaaaaaaaa>(op);
		jumpOrJSR<Jump>(addr);
	}

	inline void JitOps::op_Jscc_ea(TWord op)
	{
		RegGP ea(m_block);
		effectiveAddress<Jscc_ea>(ea, op);
		jumpIfCC<Jscc_ea, JSR>(op, ea.get().r32());
	}

	inline void JitOps::op_Jscc_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jscc_xxx, Field_aaaaaaaaaaaa>(op);
		jumpIfCC<Jscc_xxx,JSR>(op, addr);
	}

	inline void JitOps::op_Jsr_ea(TWord op)
	{
		RegGP ea(m_block);
		effectiveAddress<Jsr_ea>(ea, op);
		jumpOrJSR<JSR>(ea.get().r32());
	}

	inline void JitOps::op_Jsr_xxx(TWord op)
	{
		const auto addr = getFieldValue<Jsr_xxx, Field_aaaaaaaaaaaa>(op);
		jumpOrJSR<JSR>(addr);
	}
}
