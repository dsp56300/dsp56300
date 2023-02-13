#include "jittypes.h"

#ifdef HAVE_X86_64

#include "dspassert.h"

#include "jitops.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	asmjit::x86::CondCode JitOps::reverseCC(asmjit::x86::CondCode _cc)
	{
		if (_cc == asmjit::x86::CondCode::kZero)		return asmjit::x86::CondCode::kNotZero;
		if (_cc == asmjit::x86::CondCode::kNotZero)		return asmjit::x86::CondCode::kZero;
		if (_cc == asmjit::x86::CondCode::kP)			return asmjit::x86::CondCode::kNP;
		if (_cc == asmjit::x86::CondCode::kNP)			return asmjit::x86::CondCode::kP;

		assert(false && "invalid CC");
		return _cc;
	}

	asmjit::x86::CondCode JitOps::decode_cccc(TWord cccc)
	{
		auto ccrMaskTest = [&](const CCRMask _mask)
		{
			m_ccrRead |= _mask;
			updateDirtyCCR(_mask);
			m_asm.test(m_dspRegs.getSR(JitDspRegs::Read).r32(), asmjit::Imm(_mask));
		};

		auto ccrBitTest = [&](const CCRBit _bit)
		{
			const auto mask = static_cast<CCRMask>(1 << _bit);
			m_ccrRead |= mask;
			updateDirtyCCR(mask);
			m_asm.bitTest(m_dspRegs.getSR(JitDspRegs::Read).r32(), _bit);
		};

		switch (cccc)
		{
		case CCCC_CarrySet:											// CC(LO)		Carry Set	(lower)
			ccrBitTest(CCRB_C);
			return asmjit::x86::CondCode::kNotZero;
		case CCCC_CarryClear:										// CC(HS)		Carry Clear (higher or same)	
			ccrBitTest(CCRB_C);
			return asmjit::x86::CondCode::kZero;
		case CCCC_ExtensionSet:										// ES			Extension set	
			ccrBitTest(CCRB_E);
			return asmjit::x86::CondCode::kNotZero;
		case CCCC_ExtensionClear:									// EC			Extension clear	
			ccrBitTest(CCRB_E);
			return asmjit::x86::CondCode::kZero;
		case CCCC_Equal:											// EQ			Equal	
			ccrBitTest(CCRB_Z);
			return asmjit::x86::CondCode::kNotZero;
		case CCCC_NotEqual:											// NE			Not Equal
			ccrBitTest(CCRB_Z);
			return asmjit::x86::CondCode::kZero;
		case CCCC_LimitSet:											// LS			Limit set
			ccrBitTest(CCRB_L);
			return asmjit::x86::CondCode::kNotZero;
		case CCCC_LimitClear:										// LC			Limit clear
			ccrBitTest(CCRB_L);
			return asmjit::x86::CondCode::kZero;
		case CCCC_Minus:											// MI			Minus
			ccrBitTest(CCRB_N);
			return asmjit::x86::CondCode::kNotZero;
		case CCCC_Plus:												// PL			Plus
			ccrBitTest(CCRB_N);
			return asmjit::x86::CondCode::kZero;
		case CCCC_GreaterEqual:										// GE			Greater than or equal
			// SRB_N == SRB_V
			ccrMaskTest(static_cast<CCRMask>(CCR_N | CCR_V));
			return asmjit::x86::CondCode::kP;
		case CCCC_LessThan:											// LT			Less than
			// SRB_N != SRB_V
			ccrMaskTest(static_cast<CCRMask>(CCR_N | CCR_V));
			return asmjit::x86::CondCode::kNP;
		case CCCC_Normalized:										// NR			Normalized
			{
				// Z + (!U & !E) == 1
				const RegGP t(m_block);
				const auto tmp = t.get().r8();
				ccrMaskTest(static_cast<CCRMask>(CCR_U | CCR_E));
				m_asm.set(asmjit::x86::CondCode::kNotZero, tmp);
				const RegGP r(m_block);
				ccr_getBitValue(r, CCRB_Z);
				m_asm.add(tmp, r.get().r8());
				m_asm.test_(tmp, asmjit::Imm(1));
				return asmjit::x86::CondCode::kNotZero;
			}
		case CCCC_NotNormalized:									// NN			Not normalized
			{
				// Z + (!U & !E) == 0
				const RegGP t(m_block);
				const auto tmp = t.get().r8();
				ccrMaskTest(static_cast<CCRMask>(CCR_U | CCR_E));
				m_asm.set(asmjit::x86::CondCode::kNotZero, tmp);
				const RegGP r(m_block);
				ccr_getBitValue(r, CCRB_Z);
				m_asm.add(tmp, r.get().r8());
				m_asm.test_(tmp, asmjit::Imm(1));
				return asmjit::x86::CondCode::kZero;
			}
		case CCCC_GreaterThan:										// GT			Greater than
			{
				// (SRB_Z + (SRB_N != SRB_V)) == 0
				ccrMaskTest(static_cast<CCRMask>(CCR_Z | CCR_N | CCR_V));
				return asmjit::x86::CondCode::kParityEven;
			}
		case CCCC_LessEqual:										// LE			Less than or equal
			{
				// (SRB_Z + (SRB_N != SRB_V)) == 1
				ccrMaskTest(static_cast<CCRMask>(CCR_Z | CCR_N | CCR_V));
				return asmjit::x86::CondCode::kParityOdd;
			}
		default:
			assert(0 && "invalid CCCC value");
			return asmjit::x86::CondCode::kMaxValue;
		}
	}

	void JitOps::decode_cccc(const JitRegGP& _dst, const TWord cccc)
	{
		const auto cc = decode_cccc(cccc);
		m_asm.set(cc, _dst.r8());
	}
}

#endif