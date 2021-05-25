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
}
