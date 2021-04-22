#pragma once

#include <sstream>
#include <string>
#include <iomanip>

namespace Logging
{
	void g_logToConsole( const std::string& _s );
	void g_logToFile( const std::string& _s );
}

#define LOGTOCONSOLE(ss)	{ Logging::g_logToConsole( (ss).str() ); }
#define LOGTOFILE(ss)		{ Logging::g_logToFile( (ss).str() ); }

#define LOG(S)																												\
{																															\
	std::stringstream ss;	ss << __FUNCTION__ << "@" << __LINE__ << ": " << S;												\
																															\
	LOGTOCONSOLE(ss)																										\
}
#define LOGF(S)																												\
{																															\
	std::stringstream ss;	ss << S;																						\
																															\
	LOGTOFILE(ss)																											\
}

#define HEX(S)			std::hex << std::setfill('0') << std::setw(6) << S
