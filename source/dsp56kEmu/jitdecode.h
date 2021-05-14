#pragma once
#include "jitops.h"
#include "types.h"
#include "jitblock.h"

namespace dsp56k
{
	void JitOps::decode_cccc(JitReg& _dst, const TWord cccc)
	{
		switch( cccc )
		{
		case CCCC_CarrySet:									// CC(LO)		Carry Set	(lower)
			sr_getBitValue(_dst, SRB_C);
			break;
		case CCCC_CarryClear:								// CC(HS)		Carry Clear (higher or same)	
			sr_getBitValue(_dst, SRB_C);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_ExtensionSet:								// ES			Extension set	
			sr_getBitValue(_dst, SRB_E);
			break;
		case CCCC_ExtensionClear:							// EC			Extension clear	
			sr_getBitValue(_dst, SRB_E);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_Equal:									// EQ			Equal	
			sr_getBitValue(_dst, SRB_Z);
			break;
		case CCCC_NotEqual:									// NE			Not Equal
			sr_getBitValue(_dst, SRB_Z);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_LimitSet:									// LS			Limit set
			sr_getBitValue(_dst, SRB_L);
			break;
		case CCCC_LimitClear:								// LC			Limit clear
			sr_getBitValue(_dst, SRB_L);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_Minus:									// MI			Minus
			sr_getBitValue(_dst, SRB_N);
			break;
		case CCCC_Plus:										// PL			Plus
			sr_getBitValue(_dst, SRB_N);
			m_asm.xor_(_dst, asmjit::Imm(1));
			break;
		case CCCC_GreaterEqual:								// GE			Greater than or equal
			{
				// SRB_N == SRB_V
				sr_getBitValue(_dst, SRB_N);
				const RegGP r(m_block);
				sr_getBitValue(r, SRB_V);
				m_asm.cmp(_dst.r8(), r.get().r8());
				m_asm.sete(_dst);
			}
			break;
		case CCCC_LessThan:									// LT			Less than
			{
				// SRB_N != SRB_V
				sr_getBitValue(_dst, SRB_N);
				const RegGP r(m_block);
				sr_getBitValue(r, SRB_V);
				m_asm.cmp(_dst.r8(), r.get().r8());
				m_asm.setne(_dst);
			}
			break;
		case CCCC_Normalized:								// NR			Normalized
			{
				// (SRB_Z + ((!SRB_U) | (!SRB_E))) == 1
				sr_getBitValue(_dst, SRB_U);
				m_asm.xor_(_dst, asmjit::Imm(1));
				const RegGP r(m_block);
				sr_getBitValue(r, SRB_E);
				m_asm.xor_(r, asmjit::Imm(1));
				m_asm.and_(_dst.r8(), r.get().r8());
				sr_getBitValue(r, SRB_Z);
				m_asm.add(_dst.r8(), r.get().r8());
				m_asm.cmp(_dst.r8(), asmjit::Imm(1));
				m_asm.sete(_dst);
			}
			break;
		case CCCC_NotNormalized:							// NN			Not normalized
			{
				// (SRB_Z + ((!SRB_U) | !SRB_E)) == 0
				sr_getBitValue(_dst, SRB_U);
				m_asm.xor_(_dst, asmjit::Imm(1));
				const RegGP r(m_block);
				sr_getBitValue(r, SRB_E);
				m_asm.xor_(r, asmjit::Imm(1));
				m_asm.and_(_dst.r8(), r.get().r8());
				sr_getBitValue(r, SRB_Z);
				m_asm.add(_dst.r8(), r.get().r8());
				m_asm.cmp(_dst.r8(), asmjit::Imm(0));
				m_asm.sete(_dst);
			}
			break;
		case CCCC_GreaterThan:								// GT			Greater than
			{
				// (SRB_Z + (SRB_N != SRB_V)) == 0
				sr_getBitValue(_dst, SRB_N);
				const RegGP r(m_block);
				sr_getBitValue(r, SRB_V);
				m_asm.xor_(_dst.r8(), r.get().r8());
				sr_getBitValue(r, SRB_Z);
				m_asm.add(_dst.r8(), r.get().r8());
				m_asm.cmp(_dst.r8(), asmjit::Imm(0));
				m_asm.sete(_dst);
			}
			break;
		case CCCC_LessEqual:								// LE			Less than or equal
			{
				// (SRB_Z + (SRB_N != SRB_V)) == 1
				sr_getBitValue(_dst, SRB_N);
				const RegGP r(m_block);
				sr_getBitValue(r, SRB_V);
				m_asm.xor_(_dst.r8(), r.get().r8());
				sr_getBitValue(r, SRB_Z);
				m_asm.add(_dst.r8(), r.get().r8());
				m_asm.cmp(_dst.r8(), asmjit::Imm(1));
				m_asm.sete(_dst);
			}
			break;
		default:
			assert( 0 && "invalid CCCC value" );
		}
	}

	void JitOps::decode_dddddd_read( const JitReg32& _dst, const TWord _dddddd )
	{
		const auto i = _dddddd & 0x3f;
		switch( i )
		{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:	m_dspRegs.getXY0(_dst, 0);	break;
		case 0x01:	m_dspRegs.getXY1(_dst, 0);	break;
		case 0x02:	m_dspRegs.getXY0(_dst, 1);	break;
		case 0x03:	m_dspRegs.getXY1(_dst, 1);	break;
		// 0001DD - 4 registers in data ALU
		case 0x04:	m_dspRegs.getXY0(_dst, 0);	break;;
		case 0x05:	m_dspRegs.getXY1(_dst, 0);	break;;
		case 0x06:	m_dspRegs.getXY0(_dst, 1);	break;;
		case 0x07:	m_dspRegs.getXY1(_dst, 1);	break;;

		// 001DDD - 8 accumulators in data ALU
		case 0x08:	m_dspRegs.getALU0(_dst, 0);	break;
		case 0x09:	m_dspRegs.getALU0(_dst, 1);	break;
		case 0x0a:
		case 0x0b:
			{
				const auto aluIndex = i - 0x0a;
				m_dspRegs.getALU2signed(_dst, aluIndex);
			}
			break;
		case 0x0c:	m_dspRegs.getALU1(_dst, 0);	break;
		case 0x0d:	m_dspRegs.getALU1(_dst, 1);	break;
		case 0x0e:	
		case 0x0f:
			{
				const auto aluIndex = i - 0x0e;
				transferAluTo24(_dst, aluIndex);
			}
			break;
		// 010TTT - 8 address registers in AGU
		case 0x10:	m_dspRegs.getR(_dst, 0); break;
		case 0x11:	m_dspRegs.getR(_dst, 1); break;
		case 0x12:	m_dspRegs.getR(_dst, 2); break;
		case 0x13:	m_dspRegs.getR(_dst, 3); break;
		case 0x14:	m_dspRegs.getR(_dst, 4); break;
		case 0x15:	m_dspRegs.getR(_dst, 5); break;
		case 0x16:	m_dspRegs.getR(_dst, 6); break;
		case 0x17:	m_dspRegs.getR(_dst, 7); break;

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	m_dspRegs.getN(_dst, 0); break;
		case 0x19:	m_dspRegs.getN(_dst, 1); break;
		case 0x1a:	m_dspRegs.getN(_dst, 2); break;
		case 0x1b:	m_dspRegs.getN(_dst, 3); break;
		case 0x1c:	m_dspRegs.getN(_dst, 4); break;
		case 0x1d:	m_dspRegs.getN(_dst, 5); break;
		case 0x1e:	m_dspRegs.getN(_dst, 6); break;
		case 0x1f:	m_dspRegs.getN(_dst, 7); break;

		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	m_dspRegs.getM(_dst, 0); break;
		case 0x21:	m_dspRegs.getM(_dst, 1); break;
		case 0x22:	m_dspRegs.getM(_dst, 2); break;
		case 0x23:	m_dspRegs.getM(_dst, 3); break;
		case 0x24:	m_dspRegs.getM(_dst, 4); break;
		case 0x25:	m_dspRegs.getM(_dst, 5); break;
		case 0x26:	m_dspRegs.getM(_dst, 6); break;
		case 0x27:	m_dspRegs.getM(_dst, 7); break;
		
		// 101EEE - 1 address register in AGU
		case 0x2a:	m_dspRegs.getEP(_dst);	break;

		// 110VVV - 2 program controller registers
		case 0x30:	m_dspRegs.getVBA(_dst);	break;
		case 0x31:	m_dspRegs.getSC(_dst);	break;

		// 111GGG - 8 program controller registers
		case 0x38:	m_dspRegs.getSZ(_dst);	break;
		case 0x39:	getSR(_dst);			break;
		case 0x3a:	m_dspRegs.getOMR(_dst);	break;
		case 0x3b:	m_dspRegs.getSP(_dst);	break;
		case 0x3c:	m_dspRegs.getSSH(_dst);	break;
		case 0x3d:	m_dspRegs.getSSL(_dst);	break;
		case 0x3e:	m_dspRegs.getLA(_dst);	break;
		case 0x3f:	m_dspRegs.getLC(_dst);	break;
		default:
			assert(0 && "invalid dddddd value");
		}
	}

	void JitOps::decode_dddddd_write(const TWord _dddddd, const JitReg32& _dst)
	{
		const auto i = _dddddd & 0x3f;
		switch( i )
		{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:	m_dspRegs.setXY0(0, _dst);	break;
		case 0x01:	m_dspRegs.setXY1(0, _dst);	break;
		case 0x02:	m_dspRegs.setXY0(1, _dst);	break;
		case 0x03:	m_dspRegs.setXY1(1, _dst);	break;
		// 0001DD - 4 registers in data ALU
		case 0x04:	m_dspRegs.setXY0(0, _dst);	break;;
		case 0x05:	m_dspRegs.setXY1(0, _dst);	break;;
		case 0x06:	m_dspRegs.setXY0(1, _dst);	break;;
		case 0x07:	m_dspRegs.setXY1(1, _dst);	break;;

		// 001DDD - 8 accumulators in data ALU
		case 0x08:	m_dspRegs.setALU0(0, _dst);	break;
		case 0x09:	m_dspRegs.setALU0(0, _dst);	break;
		case 0x0a:
		case 0x0b:
			{
				const auto aluIndex = i - 0x0a;
				m_dspRegs.setALU(aluIndex, _dst);
				m_asm.sal(_dst, asmjit::Imm(8));
				m_asm.sar(_dst, asmjit::Imm(40));
				m_asm.and_(_dst, asmjit::Imm(0xffffff));
			}
			break;
		case 0x0c:	m_dspRegs.setALU1(0, _dst);	break;
		case 0x0d:	m_dspRegs.setALU1(1, _dst);	break;
		case 0x0e:	
		case 0x0f:
			{
				const auto aluIndex = i - 0x0e;
				transferAluTo24(_dst, aluIndex);
			}
			break;
		// 010TTT - 8 address registers in AGU
		case 0x10:	m_dspRegs.setR(0, _dst); break;
		case 0x11:	m_dspRegs.setR(1, _dst); break;
		case 0x12:	m_dspRegs.setR(2, _dst); break;
		case 0x13:	m_dspRegs.setR(3, _dst); break;
		case 0x14:	m_dspRegs.setR(4, _dst); break;
		case 0x15:	m_dspRegs.setR(5, _dst); break;
		case 0x16:	m_dspRegs.setR(6, _dst); break;
		case 0x17:	m_dspRegs.setR(7, _dst); break;

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	m_dspRegs.setN(0, _dst); break;
		case 0x19:	m_dspRegs.setN(1, _dst); break;
		case 0x1a:	m_dspRegs.setN(2, _dst); break;
		case 0x1b:	m_dspRegs.setN(3, _dst); break;
		case 0x1c:	m_dspRegs.setN(4, _dst); break;
		case 0x1d:	m_dspRegs.setN(5, _dst); break;
		case 0x1e:	m_dspRegs.setN(6, _dst); break;
		case 0x1f:	m_dspRegs.setN(7, _dst); break;
								   
		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	m_dspRegs.setM(0, _dst); break;
		case 0x21:	m_dspRegs.setM(1, _dst); break;
		case 0x22:	m_dspRegs.setM(2, _dst); break;
		case 0x23:	m_dspRegs.setM(3, _dst); break;
		case 0x24:	m_dspRegs.setM(4, _dst); break;
		case 0x25:	m_dspRegs.setM(5, _dst); break;
		case 0x26:	m_dspRegs.setM(6, _dst); break;
		case 0x27:	m_dspRegs.setM(7, _dst); break;
		
		// 101EEE - 1 address register in AGU
		case 0x2a:	m_dspRegs.setEP(_dst);	break;

		// 110VVV - 2 program controller registers
		case 0x30:	m_dspRegs.setVBA(_dst);	break;
		case 0x31:	m_dspRegs.setSC(_dst);	break;

		// 111GGG - 8 program controller registers
		case 0x38:	m_dspRegs.setSZ(_dst);	break;
		case 0x39:	setSR(_dst);	break;
		case 0x3a:	m_dspRegs.setOMR(_dst);	break;
		case 0x3b:	m_dspRegs.setSP(_dst);	break;
		case 0x3c:	m_dspRegs.setSSH(_dst);	break;
		case 0x3d:	m_dspRegs.setSSL(_dst);	break;
		case 0x3e:	m_dspRegs.setLA(_dst);	break;
		case 0x3f:	m_dspRegs.setLC(_dst);	break;
		default:
			assert(0 && "invalid dddddd value");
		}
	}

	void JitOps::decode_EE_read(RegGP& dst, TWord _ee)
	{
		switch (_ee)
		{
		case 0: return getMR(dst);
		case 1: return getCCR(dst);
		case 2: return getCOM(dst);
		case 3: return getEOM(dst);
		default:
			assert(0 && "invalid EE value");
		}
	}

	void JitOps::decode_EE_write(const JitReg64& _src, TWord _ee)
	{
		switch (_ee)
		{
		case 0: return setMR(_src);
		case 1: return setCCR(_src);
		case 2: return setCOM(_src);
		case 3: return setEOM(_src);
		default:
			assert(0 && "invalid EE value");
		}
	}

	void JitOps::decode_JJJ_read_56(const JitReg64 _dst, const TWord _jjj, const bool _b) const
	{
		switch (_jjj)
		{
		case 0:
		case 1:			m_dspRegs.getALU(_dst, _b ? 1 : 0);			break;
		case 2:			XYto56(_dst, 0);							break;
		case 3: 		XYto56(_dst, 1);							break;
		case 4:			XY0to56(_dst, 0);							break;
		case 5: 		XY0to56(_dst, 1);							break;
		case 6:			XY1to56(_dst, 0);							break;
		case 7: 		XY1to56(_dst, 1);							break;
		default:
			assert(0 && "unreachable, invalid JJJ value");
		}
	}

	void JitOps::decode_JJ_read(const JitReg64 _dst, TWord jj) const
	{
		switch (jj)
		{
		case 0:
		case 1:
			m_dspRegs.getXY0(_dst, jj);
			break;
		case 2: 
		case 3:
			m_dspRegs.getXY1(_dst, jj - 2);
			break;
		default:
			assert(0 && "unreachable, invalid JJ value");
		}
	}

	void JitOps::decode_sss_read(const JitReg64 _dst, const TWord _sss) const
	{
		switch( _sss )
		{
		case 2:		m_dspRegs.getALU1(_dst, 0);			break;
		case 3:		m_dspRegs.getALU1(_dst, 1);			break;
		case 4:		m_dspRegs.getXY0(_dst, 0);			break;
		case 5:		m_dspRegs.getXY0(_dst, 1);			break;
		case 6:		m_dspRegs.getXY1(_dst, 0);			break;
		case 7:		m_dspRegs.getXY1(_dst, 1);			break;
		case 0:
		case 1:
		default:
			assert( 0 && "invalid sss value" );
		}
	}
}
