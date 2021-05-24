#pragma once

#include "jitops.h"

namespace dsp56k
{
	template <> inline void JitOps::braOrBsr<Bra>(int _offset)	{ jmp(m_pcCurrentOp + _offset); }
	template <> inline void JitOps::braOrBsr<Bsr>(int _offset)	{ jsr(m_pcCurrentOp + _offset); }

	template<Instruction Inst, BraMode BMode, ExpectedBitValue BitValue> void JitOps::braIfBitTestMem(const TWord op)
	{
		const auto addr = pcRelativeAddressExt<Inst>();

		const auto end = m_asm.newLabel();

		m_dspRegs.notifyBeginBranch();

		bitTestMemory<Inst>(op);

		if(BitValue == BitSet)
			m_asm.jnc(end);
		else if(BitValue == BitClear)
			m_asm.jc(end);

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

		bitTest<Inst>(op, r);

		if(BitValue == BitSet)
			m_asm.jnc(end);
		else if(BitValue == BitClear)
			m_asm.jc(end);

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
}
