// Deterministic regression test for the SpscSemaphoreWithCount batched-notify accounting.
//
// The bug: notify(_count) posted _count inner credits whenever the count was in deficit - even when the
// deficit was smaller than the batch. The surplus credits sat in the inner semaphore forever, and a later
// wait() that went into deficit consumed one and RETURNED ALTHOUGH NOTHING HAD BEEN PRODUCED. In the audio
// rings (batched emplace_back/pop_front sharing semaphores with single-frame pushes and pops) that made a
// consumer read a frame that was not there: intermittently corrupted audio, desynced ring counters, and a
// permanently wedged pipeline (the intermittent multi-instance deadlock).
//
// The scenario is reproduced deterministically: park a consumer in deficit, batch-notify far more than the
// deficit, drain the legitimate credits, then verify that the next deficit wait actually BLOCKS.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "semaphore.h"

int main()
{
	dsp56k::SpscSemaphoreWithCount sem;

	constexpr int batch = 96;	// the ESSI1 codec feed pushes chunks this size

	std::atomic<bool> parked{false};
	std::atomic<bool> drained{false};
	std::atomic<bool> leaked{false};

	std::thread consumer([&]
	{
		// 1: go into deficit (count 0 -> -1) and park
		parked.store(true);
		sem.wait();

		// 2: consume the legitimate remainder of the batch
		for(int i = 0; i < batch - 1; ++i)
			sem.wait();

		drained.store(true);

		// 3: no notify is pending now. This wait MUST block; with the broken accounting it consumes one of
		// the spurious inner credits minted in step 1's batch notify and sails straight through.
		sem.wait();
		leaked.store(true);
	});

	while(!parked.load())
		std::this_thread::yield();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));	// let the consumer actually park in the deficit wait

	// batch-notify into a deficit of exactly 1
	sem.notify(static_cast<uint32_t>(batch));

	while(!drained.load())
		std::this_thread::yield();

	// give a leak ample time to show - the broken code passes through within microseconds
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const bool fail = leaked.load();
	if(fail)
		std::cerr << "FAILED: wait() returned without a matching notify - spurious credits were minted by the batch notify" << std::endl;

	// release the consumer (on the fixed code it is still parked in step 3)
	sem.notify();
	consumer.join();

	if(fail)
		return 1;

	std::cout << "PASSED" << std::endl;
	return 0;
}
