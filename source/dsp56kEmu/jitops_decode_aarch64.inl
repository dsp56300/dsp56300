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
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			break;
		case CCCC_ExtensionSet:								// ES			Extension set	
			ccr_getBitValue(_dst, CCRB_E);
			break;
		case CCCC_ExtensionClear:							// EC			Extension clear	
			ccr_getBitValue(_dst, CCRB_E);
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			break;
		case CCCC_Equal:									// EQ			Equal	
			ccr_getBitValue(_dst, CCRB_Z);
			break;
		case CCCC_NotEqual:									// NE			Not Equal
			ccr_getBitValue(_dst, CCRB_Z);
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			break;
		case CCCC_LimitSet:									// LS			Limit set
			ccr_getBitValue(_dst, CCRB_L);
			break;
		case CCCC_LimitClear:								// LC			Limit clear
			ccr_getBitValue(_dst, CCRB_L);
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			break;
		case CCCC_Minus:									// MI			Minus
			ccr_getBitValue(_dst, CCRB_N);
			break;
		case CCCC_Plus:										// PL			Plus
			ccr_getBitValue(_dst, CCRB_N);
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			break;
		case CCCC_GreaterEqual:								// GE			Greater than or equal
		{
			// SRB_N == SRB_V
			const RegGP r(m_block);
			m_asm.mov(_dst, asmjit::a64::xzr);
			m_asm.mov(r, asmjit::a64::xzr);
			ccr_getBitValue(_dst, CCRB_N);
			ccr_getBitValue(r, CCRB_V);
			m_asm.cmp(_dst, r.get());
			m_asm.cset(_dst, asmjit::arm::CondCode::kZero);
		}
		break;
		case CCCC_LessThan:									// LT			Less than
		{
			// SRB_N != SRB_V
			const RegGP r(m_block);
			m_asm.mov(_dst, asmjit::a64::xzr);
			m_asm.mov(r, asmjit::a64::xzr);
			ccr_getBitValue(_dst, CCRB_N);
			ccr_getBitValue(r, CCRB_V);
			m_asm.eor(_dst, _dst, r.get());
		}
		break;
		case CCCC_Normalized:								// NR			Normalized
		{
			// (SRB_Z + ((!SRB_U) | (!SRB_E))) == 1
			const RegGP r(m_block);
			m_asm.mov(_dst, asmjit::a64::xzr);
			m_asm.mov(r, asmjit::a64::xzr);
			ccr_getBitValue(_dst, CCRB_U);
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			ccr_getBitValue(r, CCRB_E);
			m_asm.eor(r, r, asmjit::Imm(1));
			m_asm.and_(_dst, r.get());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst, r.get());
			m_asm.cmp(_dst, _dst, asmjit::Imm(1));
			m_asm.cset(_dst, asmjit::arm::CondCode::kZero);
		}
		break;
		case CCCC_NotNormalized:							// NN			Not normalized
		{
			// (SRB_Z + ((!SRB_U) | !SRB_E)) == 0
			const RegGP r(m_block);
			m_asm.mov(_dst, asmjit::a64::xzr);
			m_asm.mov(r, asmjit::a64::xzr);
			ccr_getBitValue(_dst, CCRB_U);
			m_asm.eor(_dst, _dst, asmjit::Imm(1));
			ccr_getBitValue(r, CCRB_E);
			m_asm.eor(r, r, asmjit::Imm(1));
			m_asm.and_(_dst, r.get());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst, r.get());
			m_asm.test(_dst);
			m_asm.cset(_dst, asmjit::arm::CondCode::kZero);
		}
		break;
		case CCCC_GreaterThan:								// GT			Greater than
		{
			// (SRB_Z + (SRB_N != SRB_V)) == 0
			const RegGP r(m_block);
			m_asm.mov(_dst, asmjit::a64::xzr);
			m_asm.mov(r, asmjit::a64::xzr);

			ccr_getBitValue(_dst, CCRB_N);
			ccr_getBitValue(r, CCRB_V);

			m_asm.eor(_dst, _dst, r.get());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.adds(_dst, _dst, r.get());
			m_asm.cset(_dst, asmjit::arm::CondCode::kZero);
		}
		break;
		case CCCC_LessEqual:								// LE			Less than or equal
		{
			// (SRB_Z + (SRB_N != SRB_V)) == 1
			const RegGP r(m_block);
			m_asm.mov(_dst, asmjit::a64::xzr);
			m_asm.mov(r, asmjit::a64::xzr);

			ccr_getBitValue(_dst, CCRB_N);
			ccr_getBitValue(r, CCRB_V);

			m_asm.eor(_dst, _dst, r.get());
			ccr_getBitValue(r, CCRB_Z);
			m_asm.add(_dst, _dst, r.get());
			m_asm.cmp(_dst, asmjit::Imm(1));
			m_asm.cset(_dst, asmjit::arm::CondCode::kZero);
		}
		break;
		default:
			assert(0 && "invalid CCCC value");
		}
	}
}
