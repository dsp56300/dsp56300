#pragma once
#include "jitops.h"
#include "types.h"

namespace dsp56k
{

	void JitOps::decode_JJJ_read_56(asmjit::x86::Gpq dst, TWord jjj, bool b)
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
}
