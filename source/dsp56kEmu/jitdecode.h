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
