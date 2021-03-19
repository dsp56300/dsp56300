#include "pch.h"

#include "logging.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>

void g_logWin32( const std::string& _s )
{
	::OutputDebugStringA( (_s + "\n").c_str() );
}

#endif
