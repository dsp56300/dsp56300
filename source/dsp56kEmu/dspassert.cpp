#include "dspassert.h"

#include <string>

#include "logging.h"

#ifndef _WIN32
#define SUPPORT_BACKTRACE
#endif

#ifdef SUPPORT_BACKTRACE
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#endif

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

#ifdef SUPPORT_BACKTRACE
		void* entries[64];
		
		// get void*'s for all entries on the stack
		const auto size = backtrace(entries, std::size(entries));

		// print out all the frames to stderr
		backtrace_symbols_fd(entries, size, STDERR_FILENO);
#endif

		throw std::runtime_error(msg);
#endif
	}
}
