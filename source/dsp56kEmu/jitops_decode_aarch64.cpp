#include "jittypes.h"

#ifdef HAVE_ARM64

#include "dsp56kBase/dspassert.h"

#include "jitops.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	asmjit::arm::CondCode JitOps::reverseCC(asmjit::arm::CondCode _cc)
	{
		if (_cc == asmjit::arm::CondCode::kZero)		return asmjit::arm::CondCode::kNotZero;
		if (_cc == asmjit::arm::CondCode::kNotZero)		return asmjit::arm::CondCode::kZero;

		assert(false && "invalid CC");
		return _cc;
	}

	asmjit::arm::CondCode JitOps::decode_cccc(const TWord cccc)
	{
		auto ccrBitTest = [&](const CCRBit _bit)
		{
			const auto mask = static_cast<CCRMask>(1 << _bit);
			m_ccrRead |= mask;
			updateDirtyCCR(mask);
			m_asm.bitTest(r32(m_dspRegs.getSR(JitDspRegs::Read)), _bit);
		};

		switch (cccc)
		{
		case CCCC_CarrySet:									// CC(LO)		Carry Set	(lower)
			ccrBitTest(CCRB_C);
			return asmjit::arm::CondCode::kNotZero;
		case CCCC_CarryClear:								// CC(HS)		Carry Clear (higher or same)	
			ccrBitTest(CCRB_C);
			return asmjit::arm::CondCode::kZero;
		case CCCC_ExtensionSet:								// ES			Extension set	
			ccrBitTest(CCRB_E);
			return asmjit::arm::CondCode::kNotZero;
		case CCCC_ExtensionClear:							// EC			Extension clear	
			ccrBitTest(CCRB_E);
			return asmjit::arm::CondCode::kZero;
		case CCCC_Equal:									// EQ			Equal	
			ccrBitTest(CCRB_Z);
			return asmjit::arm::CondCode::kNotZero;
		case CCCC_NotEqual:									// NE			Not Equal
			ccrBitTest(CCRB_Z);
			return asmjit::arm::CondCode::kZero;
		case CCCC_LimitSet:									// LS			Limit set
			ccrBitTest(CCRB_L);
			return asmjit::arm::CondCode::kNotZero;
		case CCCC_LimitClear:								// LC			Limit clear
			ccrBitTest(CCRB_L);
			return asmjit::arm::CondCode::kZero;
		case CCCC_Minus:									// MI			Minus
			ccrBitTest(CCRB_N);
			return asmjit::arm::CondCode::kNotZero;
		case CCCC_Plus:										// PL			Plus
			ccrBitTest(CCRB_N);
			return asmjit::arm::CondCode::kZero;
		case CCCC_GreaterEqual:								// GE			Greater than or equal
			{
				// SRB_N == SRB_V
				const RegGP r(m_block);
				const RegGP dst(m_block);
				ccr_getBitValue(dst, CCRB_N);
				ccr_getBitValue(r, CCRB_V);
				m_asm.cmp(dst, r.get());
				return asmjit::arm::CondCode::kZero;
			}
		case CCCC_LessThan:									// LT			Less than
			{
				// SRB_N != SRB_V
				const RegGP r(m_block);
				const RegGP dst(m_block);
				ccr_getBitValue(dst, CCRB_N);
				ccr_getBitValue(r, CCRB_V);
				m_asm.cmp(dst, r);
				return asmjit::arm::CondCode::kNotZero;
			}
		case CCCC_Normalized:								// NR			Normalized
		case CCCC_NotNormalized:							// NN			Not normalized
			{
				// NR is Z | (!U & !E). Requiring Z, U and E to all be clear is a different function: it
				// answered "not normalized" for every state with Z set, 128 of the 256 CCR values, measured
				// against the simulator, which takes jnr in 160 states where this took it in 32. A zero
				// accumulator is normalized by definition.
				//
				// Test Z first and drop SR to zero when it is set, so the second test only ever sees U and E
				// in the states where they still matter. Three instructions and one pool register, against
				// the six and two of the bit-by-bit version this replaces.
				m_ccrRead |= static_cast<CCRMask>(CCR_Z | CCR_U | CCR_E);
				updateDirtyCCR(static_cast<CCRMask>(CCR_Z | CCR_U | CCR_E));

				const RegGP t(m_block);
				const auto sr = r32(m_dspRegs.getSR(JitDspRegs::Read));

				m_asm.tst(sr, asmjit::Imm(CCR_Z));
				m_asm.csel(r32(t), asmjit::a64::regs::wzr, sr, asmjit::arm::CondCode::kNotZero);
				m_asm.tst(r32(t), asmjit::Imm(CCR_U | CCR_E));

				return cccc == CCCC_Normalized ? asmjit::arm::CondCode::kZero : asmjit::arm::CondCode::kNotZero;
			}
		case CCCC_GreaterThan:								// GT			Greater than
			{
				// (SRB_Z + (SRB_N != SRB_V)) == 0
				const RegGP r(m_block);
				const RegGP dst(m_block);

				ccr_getBitValue(dst, CCRB_N);
				ccr_getBitValue(r, CCRB_V);

				m_asm.eor(dst, dst, r.get());
				ccr_getBitValue(r, CCRB_Z);
				m_asm.adds(dst, dst, r.get());
				return asmjit::arm::CondCode::kZero;
			}
		case CCCC_LessEqual:								// LE			Less than or equal
			{
				// (SRB_Z + (SRB_N != SRB_V)) != 0 - the inverse of GT above.
				//
				// NOT == 1: the sum is 2 when Z is set AND N differs from V, which is exactly what an ASL
				// that overflows to zero produces. Comparing against 1 made LE false there while GT was
				// also false, so the two stopped being complements and this back end skipped conditional
				// work that the simulator and x64 both perform.
				const RegGP r(m_block);
				const RegGP dst(m_block);

				ccr_getBitValue(dst, CCRB_N);
				ccr_getBitValue(r, CCRB_V);

				m_asm.eor(dst, dst, r.get());
				ccr_getBitValue(r, CCRB_Z);
				m_asm.adds(dst, dst, r.get());
				return asmjit::arm::CondCode::kNotZero;
			}
		default:
			assert(0 && "invalid CCCC value");
			return asmjit::arm::CondCode::kMaxValue;
		}
	}

	void JitOps::decode_cccc(const JitRegGP& _dst, const TWord cccc)
	{
		const auto cc = decode_cccc(cccc);
		m_asm.cset(_dst, cc);
	}
}

#endif
