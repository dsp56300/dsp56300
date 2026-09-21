#pragma once

namespace dsp56k
{
	// Facade for external profiler APIs. Backends are selected at compile time in profiler.cpp, every call is a
	// no-op if no backend is compiled in or the profiler is not present at runtime.
	//
	// Note that this is not the JIT block symbolization path, that one is JitProfilingSupport. Only profilers that
	// can be told about dynamically generated code belong there (VTune, perf), the ones here only get thread names
	// and instrumentation scopes.
	class Profiler
	{
	public:
		static void setThreadName(const char* _name);

		// _id identifies the scope for the whole program run and must be a static string, a string literal in
		// particular. _data is optional, may differ per call and is displayed as additional info for the scope.
		// A begin/end pair has to happen in the same function, use ProfilerScope instead of doing it by hand.
		static void beginEvent(const char* _id, const char* _data = nullptr);
		static void endEvent();
	};

	class ProfilerScope
	{
	public:
		explicit ProfilerScope(const char* _id, const char* _data = nullptr)	{ Profiler::beginEvent(_id, _data); }
		~ProfilerScope()													{ Profiler::endEvent(); }

		ProfilerScope(const ProfilerScope&) = delete;
		ProfilerScope& operator = (const ProfilerScope&) = delete;
	};
}
