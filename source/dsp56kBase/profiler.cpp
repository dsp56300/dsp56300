#include "profiler.h"

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
#include "vtuneSdk/include/ittnotify.h"
#endif

#ifdef DSP56K_USE_SUPERLUMINAL_PROFILING_API
#include <string>
#include <Windows.h>
#include "superluminalSdk/include/Superluminal/PerformanceAPI_loader.h"
#endif

namespace dsp56k
{
#ifdef DSP56K_USE_SUPERLUMINAL_PROFILING_API
	namespace
	{
		const PerformanceAPI_Functions& superluminal()
		{
			// The loader zero-initializes the function table if anything fails, so an absent profiler ends up as a
			// table full of null pointers and every call below turns into one predictable branch.
			static const PerformanceAPI_Functions s_functions = []
			{
				PerformanceAPI_Functions functions{};

				// Always an absolute path to the install location. Loading "PerformanceAPI.dll" by name would let
				// LoadLibrary search the working directory and PATH of whatever host we are running in, which is a
				// dll planting hole that buys us nothing.
#if defined(_M_ARM64EC)
				const wchar_t* const arch = L"arm64ec";
#elif defined(_M_ARM64) || defined(__aarch64__)
				const wchar_t* const arch = L"arm64";
#elif defined(_M_X64) || defined(__x86_64__)
				const wchar_t* const arch = L"x64";
#else
				const wchar_t* const arch = L"x86";
#endif
				wchar_t programFiles[MAX_PATH]{};
				const auto len = GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);

				std::wstring path = (len > 0 && len < MAX_PATH) ? programFiles : L"C:\\Program Files";
				path += L"\\Superluminal\\Performance\\API\\dll\\";
				path += arch;
				path += L"\\PerformanceAPI.dll";

				PerformanceAPI_LoadFrom(path.c_str(), &functions);

				return functions;
			}();

			return s_functions;
		}
	}
#endif

	void Profiler::setThreadName(const char* _name)
	{
#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
		__itt_thread_set_name(_name);
#endif
#ifdef DSP56K_USE_SUPERLUMINAL_PROFILING_API
		if(const auto func = superluminal().SetCurrentThreadName)
			func(_name);
#endif
	}

	void Profiler::beginEvent(const char* _id, const char* _data)
	{
		// VTune has no counterpart here on purpose, __itt_task_begin needs a domain and interned string handles for
		// both id and data. It symbolizes the generated code itself via JitProfilingSupport, which is more useful.
#ifdef DSP56K_USE_SUPERLUMINAL_PROFILING_API
		if(const auto func = superluminal().BeginEvent)
			func(_id, _data, PERFORMANCEAPI_DEFAULT_COLOR);
#else
		(void)_id;
		(void)_data;
#endif
	}

	void Profiler::endEvent()
	{
#ifdef DSP56K_USE_SUPERLUMINAL_PROFILING_API
		if(const auto func = superluminal().EndEvent)
			func();
#endif
	}
}
