#pragma once

#include <string>

namespace dsp56k
{
	enum class ThreadPriority : int8_t
	{
		Lowest = -2,
		Low = -1,
		Normal = 0,
		High = 1,
		Highest = 2
	};

	class ThreadTools
	{
	public:
		static void setCurrentThreadName(const std::string& _name);
		static bool setCurrentThreadPriority(ThreadPriority _priority);
		// _samplerate/_blocksize describe the audio callback timing that paces the calling thread,
		// pass zeros if unknown to use conservative defaults. No-op on platforms other than macOS
		static bool setCurrentThreadRealtimeParameters(int _samplerate, int _blocksize);
	};
}
