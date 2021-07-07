#pragma once

#include <sstream>
#include <string>
#include <iomanip>

namespace Logging
{
	void g_logToConsole( const std::string& _s );
	void g_logToFile( const std::string& _s );

	template<typename ... Args>
	std::string string_format( const std::string& format, Args ... args )
	{
		// Alternative to C++20 std::format
		int size_s = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
		if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
		auto size = static_cast<size_t>( size_s );
		auto buf = std::make_unique<char[]>( size );
		snprintf( buf.get(), size, format.c_str(), args ... );
		return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
	}
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

#define LOGFMT(fmt, ...)	LOG(Logging::string_format(fmt,  ##__VA_ARGS__))

#define HEX(S)			std::hex << std::setfill('0') << std::setw(6) << S
#define HEXN(S, n)		std::hex << std::setfill('0') << std::setw(n) << (uint32_t)S
