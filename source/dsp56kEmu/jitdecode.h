#pragma once
#include "jitops.h"
#include "types.h"

namespace dsp56k
{
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

	void JitOps::decode_EE_write(const asmjit::x86::Gpq src, TWord _ee)
	{
		switch (_ee)
		{
		case 0: return setMR(src);
		case 1: return setCCR(src);
		case 2: return setCOM(src);
		case 3: return setEOM(src);
		default:
			assert(0 && "invalid EE value");
		}
	}

	void JitOps::decode_JJJ_read_56(const asmjit::x86::Gpq dst, TWord jjj, bool b) const
	{
		switch (jjj)
		{
		case 0:
		case 1:			m_dspRegs.getALU(dst, b ? 1 : 0);			break;
		case 2:			XYto56(dst, 0);								break;
		case 3: 		XYto56(dst, 1);								break;
		case 4:			XY0to56(dst, 0);							break;
		case 5: 		XY0to56(dst, 1);							break;
		case 6:			XY1to56(dst, 0);							break;
		case 7: 		XY1to56(dst, 1);							break;
		default:
			assert(0 && "unreachable, invalid JJJ value");
		}
	}

	void JitOps::decode_JJ_read(const asmjit::x86::Gpq dst, TWord jj) const
	{
		switch (jj)
		{
		case 0:
		case 1: 
			m_dspRegs.getXY(dst, jj);
			m_asm.and_(dst, asmjit::Imm(0xffffff));
			break;
		case 2: 
		case 3:
			m_dspRegs.getXY(dst, jj - 2);
			m_asm.shr(dst, asmjit::Imm(24));
			break;
		default:
			assert(0 && "unreachable, invalid JJ value");
		}
	}

}
