#pragma once

#include <sstream>
#include <string>


#ifdef _WIN32
namespace Logging
{
	void g_logWin32( const std::string& _s );
	void g_logfWin32( const std::string& _s );
}

	#define LOGWIN32(ss)	{ Logging::g_logWin32( (ss).str() ); }
	#define LOGFWIN32(ss)	{ Logging::g_logfWin32( (ss).str() ); }
#else
	#define LOGWIN32(S)		{}
	#define LOGFWIN32(S)	{}
#endif

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
