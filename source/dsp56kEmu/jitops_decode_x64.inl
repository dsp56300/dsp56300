#pragma once

#include "dspassert.h"

#include "jitops.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::decode_cccc(const JitRegGP& _dst, const TWord cccc)
	{
		switch (cccc)
		{
		case CCCC_CarrySet:									// CC(LO)		Carry Set	(lower)
			ccr_getBitValue(_dst, CCRB_C);
			break;
		case CCCC_CarryClear:								// CC(HS)		Carry Clear (higher or same)	
			ccr_getBitValue(_dst, CCRB_C);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_ExtensionSet:								// ES			Extension set	
			ccr_getBitValue(_dst, CCRB_E);
			break;
		case CCCC_ExtensionClear:							// EC			Extension clear	
			ccr_getBitValue(_dst, CCRB_E);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_Equal:									// EQ			Equal	
			ccr_getBitValue(_dst, CCRB_Z);
			break;
		case CCCC_NotEqual:									// NE			Not Equal
			ccr_getBitValue(_dst, CCRB_Z);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_LimitSet:									// LS			Limit set
			ccr_getBitValue(_dst, CCRB_L);
			break;
		case CCCC_LimitClear:								// LC			Limit clear
			ccr_getBitValue(_dst, CCRB_L);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_Minus:									// MI			Minus
			ccr_getBitValue(_dst, CCRB_N);
			break;
		case CCCC_Plus:										// PL			Plus
			ccr_getBitValue(_dst, CCRB_N);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_GreaterEqual:								// GE			Greater than or equal
		{
			// SRB_N == SRB_V
			ccr_getBitValue(_dst, CCRB_N);
			const RegGP r(m_block);
			ccr_getBitValue(r, CCRB_V);
			m_asm.cmp(_dst.r8(), r.get().r8());
			m_asm.sete(_dst);
		}
		break;
		case CCCC_LessThan:									// LT			Less than
		{
			// SRB_N != SRB_V
			ccr_getBitValue(_dst, CCRB_N);
			const RegGP r(m_block);
			ccr_getBitValue(r, CCRB_V);
			m_asm.cmp(_dst.r8(), r.get().r8());
			m_asm.setne(_dst);
		}
		break;
		case CCCC_Normalized:								// NR			Normalized
		{
			// (SRB_Z + ((!SRB_U) | (!SRB_E))) == 1
			ccr_getBitValue(_dst, CCRB_U);
			m_asm.xor_(_dst, asmjit::Imm(1));
			const RegGP r(m_block);
			ccr_getBitValue(r, CCRB_E);
			m_asm.xor_(r, asmjit::Imm(1));
			m_asm.and_(_dst.r8(), r.get().r8());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst.r8(), r.get().r8());
			m_asm.cmp(_dst.r8(), asmjit::Imm(1));
			m_asm.sete(_dst);
		}
		break;
		case CCCC_NotNormalized:							// NN			Not normalized
		{
			// (SRB_Z + ((!SRB_U) | !SRB_E)) == 0
			ccr_getBitValue(_dst, CCRB_U);
			m_asm.xor_(_dst, asmjit::Imm(1));
			const RegGP r(m_block);
			ccr_getBitValue(r, CCRB_E);
			m_asm.xor_(r, asmjit::Imm(1));
			m_asm.and_(_dst.r8(), r.get().r8());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst.r8(), r.get().r8());
			m_asm.setz(_dst);
		}
		break;
		case CCCC_GreaterThan:								// GT			Greater than
		{
			// (SRB_Z + (SRB_N != SRB_V)) == 0
			ccr_getBitValue(_dst, CCRB_N);
			const RegGP r(m_block);
			ccr_getBitValue(r, CCRB_V);
			m_asm.xor_(_dst.r8(), r.get().r8());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst.r8(), r.get().r8());
			m_asm.setz(_dst);
		}
		break;
		case CCCC_LessEqual:								// LE			Less than or equal
		{
			// (SRB_Z + (SRB_N != SRB_V)) == 1
			ccr_getBitValue(_dst, CCRB_N);
			const RegGP r(m_block);
			ccr_getBitValue(r, CCRB_V);
			m_asm.xor_(_dst.r8(), r.get().r8());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst.r8(), r.get().r8());
			m_asm.cmp(_dst.r8(), asmjit::Imm(1));
			m_asm.sete(_dst);
		}
		break;
		default:
			assert(0 && "invalid CCCC value");
		}
	}

}
