#include "logging.h"

#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

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

	static const std::string g_outfilename = buildOutfilename();

	std::unique_ptr<std::thread> g_logger;
	std::vector<std::string> g_pendingLogs;
	std::mutex g_logMutex;
	using Guard = std::lock_guard<std::mutex>;
	
	void g_logfWin32( const std::string& _s )
	{
		{
			Guard g(g_logMutex);
			g_pendingLogs.push_back(_s);
		}

		if(!g_logger)
		{
			g_logger.reset(new std::thread([]()
			{
				std::ofstream o(g_outfilename, std::ios::app);

				if(o.is_open())
				{
					while(true)
					{
						std::vector<std::string> pendingLogs;
						{							
							Guard g(g_logMutex);
							std::swap(g_pendingLogs, pendingLogs);
						}

						if(pendingLogs.empty())
							std::this_thread::sleep_for(std::chrono::milliseconds(500));

						for(auto log : pendingLogs)
							o << log << std::endl;

						o.flush();
					}
				}
			}));
		}
	}
}
#endif
