#include "jittypes.h"

#ifdef HAVE_X86_64

#include "dsp56kBase/dspassert.h"

#include "jitops.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	asmjit::x86::CondCode JitOps::reverseCC(const asmjit::x86::CondCode _cc)
	{
		return negateCond(_cc);
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
				ccrMaskTest(static_cast<CCRMask>(CCR_Z | CCR_U | CCR_E));
				return asmjit::x86::CondCode::kZero;
			}
		case CCCC_NotNormalized:									// NN			Not normalized
			{
				// Z + (!U & !E) == 0
				ccrMaskTest(static_cast<CCRMask>(CCR_Z | CCR_U | CCR_E));
				return asmjit::x86::CondCode::kNotZero;
			}
		case CCCC_GreaterThan:										// GT			Greater than
		case CCCC_LessEqual:										// LE			Less than or equal
			{
				// GT is (SRB_Z + (SRB_N != SRB_V)) == 0, LE the inverse.
				//
				// Testing SR against Z|N|V and reading the parity flag is NOT that: Z set together with
				// exactly one of N and V is two bits, i.e. even parity, so GT was taken where the DSP
				// does not take it. Simulator, tgt x0,a with the CCR preloaded: the transfer happens for
				// $00 and $0a only, while parity also fires on $06 (Z,V) and $0c (Z,N).
				//
				// Mask SR down to V,Z,N and shift right by two, then XOR back against SR. Bit 1 becomes
				// the shifted N against V, i.e. N^V, and bit 2 becomes Z, because the shifted value has
				// nothing at bit 4 any more. One test over those two bits is the whole condition. The
				// AArch64 back end computes this with two registers; only one is free here.
				// Mark these bits as READ. The block level CCR bookkeeping uses m_ccrRead to decide which
				// condition codes a later block still needs; reading SR directly here without recording
				// it let N and Z be optimised away across a block boundary. ccrMaskTest did this on our
				// behalf, the hand rolled sequence below does not.
				m_ccrRead |= static_cast<CCRMask>(CCR_V | CCR_Z | CCR_N);

				updateDirtyCCR(static_cast<CCRMask>(CCR_V | CCR_Z | CCR_N));

				// RegScratch, not RegGP: this runs inside other ops, and taking a register from the pool
				// here can evict one the caller is still holding a raw handle to. The scratch register is
				// reserved and evicts nothing.
				const RegScratch t(m_block);
				const auto sr = m_dspRegs.getSR(JitDspRegs::Read).r32();

				m_asm.mov(r32(t), sr);
				m_asm.and_(r32(t), asmjit::Imm(CCR_V | CCR_Z | CCR_N));
				m_asm.shr(r32(t), asmjit::Imm(2));
				m_asm.xor_(r32(t), sr);
				m_asm.test(r32(t), asmjit::Imm(CCR_V | CCR_Z));

				return cccc == CCCC_GreaterThan ? asmjit::x86::CondCode::kZero : asmjit::x86::CondCode::kNotZero;
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