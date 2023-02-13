#pragma once

#include <string>

namespace dsp56k
{
	enum class ThreadPriority
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
	};
}
