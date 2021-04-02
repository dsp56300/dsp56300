#include "pch.h"

#if defined(_WIN32) && defined(_DEBUG)
#	define SUPPORT_DISASSEMBLER
#endif

#ifdef SUPPORT_DISASSEMBLER
extern "C"
{
	#include "../dsp56k/PROTO563.H"
}
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
