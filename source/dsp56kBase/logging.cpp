#include "logging.h"

#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <ctime>
#define output_string(s) ::OutputDebugStringA(s)
#else
#define output_string(s) fputs(s, stdout);
#endif

namespace Logging
{
	void g_defaultLogToConsole(const std::string& _s)
	{
		output_string( (_s + "\n").c_str() );
	}

	namespace
	{
		LogFunc g_logFunc = &g_defaultLogToConsole;
	}

	void g_logToConsole( const std::string& _s)
	{
		g_logFunc(_s);
	}

	std::string buildOutfilename()
	{
		char strTime[128];

		time_t t;
		time(&t);

		tm lt;

		memcpy(&lt,localtime(&t),sizeof(tm));

		strftime( strTime, 127, "%Y-%m-%d-%H-%M-%S", &lt );

		return std::string(strTime) + ".log";
	}

	static const std::string g_outfilename = buildOutfilename();

	std::unique_ptr<std::thread> g_logger;
	std::vector<std::string> g_pendingLogs;
	std::mutex g_logMutex;
	using Guard = std::lock_guard<std::mutex>;

	std::condition_variable g_logCv;
	bool g_loggerStop = false;

	// The logger thread ran until the process ended, and destroying a joinable std::thread at static
	// destruction calls std::terminate - so every process that wrote a single line to the log file
	// aborted on exit. Stop and join it here instead. Declared after g_logger so it is destroyed
	// before it, while the mutex and the condition variable are both still alive.
	struct LoggerStopper
	{
		~LoggerStopper()
		{
			{
				Guard g(g_logMutex);
				g_loggerStop = true;
			}
			g_logCv.notify_all();

			if(g_logger && g_logger->joinable())
				g_logger->join();
		}
	};

	LoggerStopper g_loggerStopper;
	
	void g_logToFile( const std::string& _s )
	{
		// enable this to have synchronous logging, in case of a crash where you'll lose the latest logs otherwise
#if 0
		std::ofstream o(g_outfilename, std::ios::app);

		if(o.is_open())
		{
			o << _s << std::endl;
			return;
		}
#endif
//		g_logToConsole(_s);

		{
			Guard g(g_logMutex);
			g_pendingLogs.push_back(_s);
		}

		g_logCv.notify_all();

		if(!g_logger)
		{
			g_logger.reset(new std::thread([]()
			{
				std::ofstream o(g_outfilename, std::ios::app);

				if(o.is_open())
				{
					bool stop = false;

					while(!stop)
					{
						std::vector<std::string> pendingLogs;
						{
							std::unique_lock<std::mutex> lock(g_logMutex);
							g_logCv.wait_for(lock, std::chrono::milliseconds(500), []{ return g_loggerStop || !g_pendingLogs.empty(); });
							std::swap(g_pendingLogs, pendingLogs);
							stop = g_loggerStop;
						}

						for(const auto& log : pendingLogs)
							o << log << '\n';

						o.flush();
					}
				}
			}));
		}
	}

	void setLogFunc(const LogFunc _func)
	{
		g_logFunc = _func;
	}
}

