#include "logging.h"

#include <fstream>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>

#include <ctime>

namespace Logging
{
	void g_logWin32( const std::string& _s )
	{
		::OutputDebugStringA( (_s + "\n").c_str() );
	}

	std::string buildOutfilename()
	{
		char strTime[128];

		time_t t;
		time(&t);

		tm lt;
		localtime_s(&lt,&t);
		strftime( strTime, 127, "%Y-%m-%d-%H-%M-%S", &lt );

		return std::string(strTime) + ".log";
	}

	static std::string g_outfilename = buildOutfilename();

	void g_logfWin32( const std::string& _s )
	{
		std::ofstream o(g_outfilename, std::ios::app);

		if(o.is_open())
			o << _s << std::endl;
	}
}
#endif
