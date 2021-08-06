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
	void Assert::show( const char* _msg, const char* _func, int _line)
	{
#ifdef _WIN32
		LOG("DSP ASSERT: " << _msg);
		std::stringstream ss;
		ss << "@ " << _func << ", line " << _line << ": " << _msg << "\n\nBreak into the debugger?";
		const std::string msg(ss.str());
		const int res = ::MessageBoxA( nullptr, msg.c_str(), "DSP 56300 Emulator: ASSERTION FAILED", MB_YESNOCANCEL );
		switch( res )
		{
		case IDCANCEL:	exit(0);			break;
		case IDYES:		::DebugBreak();		break;
		case IDNO:							break;
		}
#else
		std::stringstream ss;
		ss << "DSP 56300 Emulator: ASSERTION FAILED @ " << _func << ", line " << _line << ": " << _msg;
		const std::string msg(ss.str());
		std::cerr << msg << std::endl;
		throw std::runtime_error(msg);
#endif
	}
}
