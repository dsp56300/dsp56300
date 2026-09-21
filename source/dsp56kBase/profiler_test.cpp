// Smoke test for the external profiler facade.
//
// Everything in Profiler goes through one lazily initialized function table that is filled by loading a dll that
// is usually not there. The failure modes are all in that plumbing and none of them need a profiler to reproduce:
// a table that is not zero-initialized calls a garbage pointer, an unguarded call crashes without the profiler
// installed, and the magic static that loads the dll is reached from several threads at once (the DSP threads all
// name themselves at startup). So: hammer every entry point from multiple threads and require it to survive.
//
// A profiler being attached is not required for this to be meaningful - without one the table is all nullptr and
// this verifies the no-op path, with one it verifies the real calls.

#include <thread>
#include <vector>

#include "profiler.h"

int main()
{
	constexpr int numThreads = 8;

	std::vector<std::thread> threads;
	threads.reserve(numThreads);

	for(int t=0; t<numThreads; ++t)
	{
		threads.emplace_back([]
		{
			dsp56k::Profiler::setThreadName("profilerTest");

			for(int i=0; i<1000; ++i)
			{
				const dsp56k::ProfilerScope outer("profilerTest::outer");
				const dsp56k::ProfilerScope inner("profilerTest::inner", "nested scope with data");
			}

			// begin/end by hand, as a profiling system with its own scope type would do it
			dsp56k::Profiler::beginEvent("profilerTest::manual");
			dsp56k::Profiler::endEvent();
		});
	}

	for(auto& t : threads)
		t.join();

	return 0;
}
