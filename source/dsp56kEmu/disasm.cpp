#include "pch.h"

#define SUPPORT_DISASSEMBLER

#ifdef SUPPORT_DISASSEMBLER
#ifdef _DEBUG
extern "C"
{
#include "../dsp56k/PROTO563.H"
}
#pragma comment(lib, "../dsp56k/CM56300.LIB")
#endif
#endif

namespace dsp56k
{
	int disassemble( char* _dst, unsigned long* _ops, const unsigned long _sr, const unsigned long _omr )
	{
#ifdef SUPPORT_DISASSEMBLER
		return dspt_unasm_563( _ops, _dst, _sr, _omr, nullptr );
#else
		return 0;
#endif
	}
}
