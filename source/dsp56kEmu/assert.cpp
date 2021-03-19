#include "pch.h"

#include "assert.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace dsp56k
{
	// _____________________________________________________________________________
	// showAssert
	//
	void Assert::show( const char* _msg )
	{
#ifdef _WIN32
		int res = ::MessageBoxA( NULL, (std::string(_msg) + "\n\nBreak into the debugger?").c_str(), "DSP 56300 Emulator: ASSERTION FAILED", MB_YESNOCANCEL );
		switch( res )
		{
		case IDCANCEL:	exit(0);			break;
		case IDYES:		::DebugBreak();		break;
		case IDNO:							break;
		}
#else
		// don't know about linux
		fputs( stderr, _msg );
#endif
	}
}
