#pragma once

#include <sstream>
#include <string>


namespace Logging
{
	void g_logWin32( const std::string& _s );
	void g_logfWin32( const std::string& _s );
}

	#define LOGWIN32(ss)	{ Logging::g_logWin32( (ss).str() ); }
	#define LOGFWIN32(ss)	{ Logging::g_logfWin32( (ss).str() ); }

//#ifndef _MASTER_BUILD
	#define LOG(S)																												\
	{																															\
		std::stringstream ss;	ss << __FUNCTION__ << "@" << __LINE__ << ": " << S;												\
																																\
		LOGWIN32(ss)																											\
	}
	#define LOGF(S)																												\
	{																															\
		std::stringstream ss;	ss << S;																						\
																																\
		LOGFWIN32(ss)																											\
	}
//#else
//	#define LOG(S)	{}
//#endif

#define HEX(S)			std::hex << std::setfill('0') << std::setw(6) << S
