#pragma once
#include "jitops.h"
#include "types.h"

namespace dsp56k
{
	void JitOps::decode_dddddd_read( JitReg32& _dst, const TWord _dddddd )
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
				m_dspRegs.getALU(_dst, aluIndex);
				m_asm.sal(_dst, asmjit::Imm(8));
				m_asm.sar(_dst, asmjit::Imm(40));
				m_asm.and_(_dst, asmjit::Imm(0xffffff));
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
		case 0x39:	m_dspRegs.getSR(_dst);	break;
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
