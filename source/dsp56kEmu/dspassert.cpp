#include "dspassert.h"

#include <string>

#include "logging.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <iostream>
#endif

namespace dsp56k
{
	// _____________________________________________________________________________
	// showAssert
	//
	void Assert::show( const char* _msg )
	{
#ifdef _WIN32
		LOG("DSP ASSERT: " << _msg);
		int res = ::MessageBoxA( NULL, (std::string(_msg) + "\n\nBreak into the debugger?").c_str(), "DSP 56300 Emulator: ASSERTION FAILED", MB_YESNOCANCEL );
		switch( res )
		{
		case IDCANCEL:	exit(0);			break;
		case IDYES:		::DebugBreak();		break;
		case IDNO:							break;
		}
#else
		std::cerr << "DSP 56300 Emulator: ASSERTION FAILED" << _msg << std::endl;
#endif
	}
}
