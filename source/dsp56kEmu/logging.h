#pragma once

#include <sstream>
#include <string>
#include <iomanip>
#include <memory>

namespace Logging
{
	void g_logToConsole( const std::string& _s );
	void g_logToFile( const std::string& _s );
}

#define LOGTOCONSOLE(ss)	{ Logging::g_logToConsole( (ss).str() ); }
#define LOGTOFILE(ss)		{ Logging::g_logToFile( (ss).str() ); }

#define LOG(S)																												\
{																															\
	std::stringstream __ss__logging_h;	__ss__logging_h << __FUNCTION__ << "@" << __LINE__ << ": " << S;					\
																															\
	LOGTOCONSOLE(__ss__logging_h)																							\
}
#define LOGF(S)																												\
{																															\
	std::stringstream __ss__logging_h;	__ss__logging_h << S;																\
																															\
	LOGTOFILE(__ss__logging_h)																								\
}

#define LOGFMT(fmt, ...)	LOG(Logging::string_format(fmt,  ##__VA_ARGS__))

#define HEX(S)			std::hex << std::setfill('0') << std::setw(6) << S
#define HEXN(S, n)		std::hex << std::setfill('0') << std::setw(n) << (uint32_t)S
