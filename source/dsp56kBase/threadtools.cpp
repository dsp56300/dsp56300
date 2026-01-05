#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "threadtools.h"

#include "logging.h"

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
#include "vtuneSdk/include/ittnotify.h"
#endif

#ifdef _WIN32
#	include <Windows.h>
#else
#	include <pthread.h>
#	include <sched.h>
#	include <sys/resource.h>
#	include <sys/syscall.h>
#	include <unistd.h>
#	include <string.h>	// strerror
#endif

#ifdef __APPLE__
#	include <mach/mach.h>
#endif

namespace dsp56k
{
#ifdef _WIN32
	constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	void SetThreadName( DWORD dwThreadID, const char* threadName)
	{
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = threadName;
		info.dwThreadID = dwThreadID;
		info.dwFlags = 0;

		__try  // NOLINT(clang-diagnostic-language-extension-token)
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info) );
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
#endif
	void ThreadTools::setCurrentThreadName(const std::string& _name)
	{
#ifdef _WIN32
		SetThreadName(-1, _name.c_str());
#elif defined(__APPLE__)
		pthread_setname_np(_name.c_str());
#else
		pthread_setname_np(pthread_self(), _name.c_str());
#endif

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
		__itt_thread_set_name(_name.c_str());
#endif
	}

	bool ThreadTools::setCurrentThreadPriority(ThreadPriority _priority)
	{
#ifdef _WIN32
		int prio;
		switch(_priority)
		{
		case ThreadPriority::Lowest:	prio = THREAD_PRIORITY_LOWEST; break;
		case ThreadPriority::Low:		prio = THREAD_PRIORITY_BELOW_NORMAL; break;
		case ThreadPriority::Normal:	prio = THREAD_PRIORITY_NORMAL; break;
		case ThreadPriority::High:		prio = THREAD_PRIORITY_ABOVE_NORMAL; break;
		case ThreadPriority::Highest:	prio = THREAD_PRIORITY_TIME_CRITICAL; break;
		default: return false;
		}
		if( !::SetThreadPriority(GetCurrentThread(), prio))
		{
			LOG("Failed to set thread priority to " << prio);
			return false;
		}
#elif defined(__APPLE__)
		const auto max = sched_get_priority_max(SCHED_OTHER);
		const auto min = sched_get_priority_min(SCHED_OTHER);
		const auto normal = (max - min) >> 1;
		const auto above = (max + normal) >> 1;
		const auto below = (min + normal) >> 1;

		int prio;
		switch(_priority)
		{
		case ThreadPriority::Lowest:	prio = min; break;
		case ThreadPriority::Low:		prio = below; break;
		case ThreadPriority::Normal:	prio = normal; break;
		case ThreadPriority::High:		prio = above; break;
		case ThreadPriority::Highest:	prio = max; break;
		default: return false;
		}

		sched_param sch_params;
		sch_params.sched_priority = prio;

		const auto id = pthread_self();

		const auto result = pthread_setschedparam(id, SCHED_OTHER, &sch_params);
		if(result)
			LOG("Failed to set thread priority to " << prio << ", error code " << result);

		if (_priority == ThreadPriority::Highest)
		{
			pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
			setCurrentThreadRealtimeParameters(0, 0);
		}
		else if (_priority == ThreadPriority::Low || _priority == ThreadPriority::Lowest)
		{
			pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
		}
#else
		// On Linux we adjust the 'nice' value of the thread
		int prio;
		switch (_priority)
		{
			case ThreadPriority::Lowest:	prio = 15;
			case ThreadPriority::Low:		prio = 10;
			case ThreadPriority::Normal:	prio = 0;
			case ThreadPriority::High:		prio = -5;
			case ThreadPriority::Highest:	prio = -10;
		}
#ifdef PRIO_THREAD
		const auto result = setpriority(PRIO_THREAD, 0, prio);
#else
		const auto tid = syscall(SYS_gettid);
		if (!tid)
		{
			LOG("Failed to get thread id for setting priority");
			return false;
		}
		const auto result = setpriority(PRIO_PROCESS, tid, prio);
#endif
		if (result != 0)
		{
			LOG("Failed to set thread priority to " << prio << ", error code " << result);
		}
#endif
		return true;
	}

	bool ThreadTools::setCurrentThreadRealtimeParameters(int _samplerate, int _blocksize)
	{
#ifdef __APPLE__
		bool usePeriod = true;
		if (_samplerate)
		{
			// set some reasonable realtime parameters for audio processing, disable fixed call frequency
			_samplerate = 44100;
			_blocksize = 2048;
			usePeriod = false;
		}
	    // Compute the nominal "period" between activations, in microseconds.
	    // Example: 44100 Hz, 1024 buffer => 23,219 us
	    double periodUsec = static_cast<double>(_blocksize) * 1'000'000.0 / static_cast<double>(_samplerate);

	    // Reasonable assumptions:
		// computation = 25% - 35% of the period
	    // constraint = equal to or slightly above the period
	    // The exact numbers aren't critical, but they should stay consistent.
	    uint32_t computation = static_cast<uint32_t>(periodUsec * 0.30);
	    uint32_t constraint  = static_cast<uint32_t>(periodUsec * 1.05);
	    uint32_t period      = usePeriod ? static_cast<uint32_t>(periodUsec) : 0;

	    // Clamp to sane limits
	    computation = std::max<uint32_t>(computation, 1000); // >= 1 ms
	    constraint = std::max<uint32_t>(constraint, computation + 1000); // Always > computation

	    // Prepare Mach real-time policy
	    thread_time_constraint_policy_data_t policy;
	    policy.period       = period;        // expected activation interval
	    policy.computation  = computation;   // estimated CPU time
	    policy.constraint   = constraint;    // must finish before this
	    policy.preemptible  = TRUE;

	    thread_port_t thread = pthread_mach_thread_np(pthread_self());
	    kern_return_t result = thread_policy_set(thread,
	                                             THREAD_TIME_CONSTRAINT_POLICY,
	                                             (thread_policy_t)&policy,
	                                             THREAD_TIME_CONSTRAINT_POLICY_COUNT);

	    if (result == KERN_SUCCESS)
	    {
			LOG("Success setting thread realtime parameters: period=" << period << " us, computation=" << computation << " us, constraint=" << constraint << " us");
	        return false;
	    }
		LOG("Failed to set thread realtime parameters, error code " << result);
#endif
		return false;
	}
}
