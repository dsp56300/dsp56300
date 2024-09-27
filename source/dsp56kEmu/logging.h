#pragma once

#include <sstream>
#include <string>
#include <iomanip>

namespace Logging
{
	typedef void (*LogFunc)(const std::string&);

	void g_logToConsole( const std::string& _s );
	void g_logToFile( const std::string& _s );
	void setLogFunc(LogFunc _func);
}

#define LOGTOCONSOLE(ss)	{ Logging::g_logToConsole( (ss).str() ); }
#define LOGTOFILE(ss)		{ Logging::g_logToFile( (ss).str() ); }

#define LOG(S)																												\
do																															\
{																															\
	std::stringstream __ss__logging_h;	__ss__logging_h << __func__ << "@" << __LINE__ << ": " << S;						\
																															\
	LOGTOCONSOLE(__ss__logging_h)																							\
}																															\
while(false)

#define LOGF(S)																												\
do																															\
{																															\
	std::stringstream __ss__logging_h;	__ss__logging_h << S;																\
																															\
	LOGTOFILE(__ss__logging_h)																								\
}																															\
while (false)

#define LOGFMT(fmt, ...)	LOG(Logging::string_format(fmt,  ##__VA_ARGS__))

#define HEX(S)			std::hex << std::setfill('0') << std::setw(6) << S
#define HEXN(S, n)		std::hex << std::setfill('0') << std::setw(n) << (uint32_t)S
