#include "ringbuffer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>

// Concurrency regression test for the blocking (Lock=true) RingBuffer, whose full/empty waits are backed
// by dsp56k::Semaphore -> dsp56k::ConditionVariable. A deliberately tiny buffer + high volume + jittered
// timing forces constant block/wake hand-offs in BOTH directions (the boot-audio ping-pong), so:
//   - a lost wakeup deadlocks the ring -> the watchdog sees no progress and fails the process, and
//   - any dropped/reordered item -> a value mismatch.
// Platform independent on purpose: this is the regression guard for the Win8 fast path, the macOS __ulock
// fast path AND the std::condition_variable fallback - all three must pass this identically.

namespace
{
	constexpr uint32_t Capacity   = 8;			// tiny on purpose: producer/consumer ping-pong at the boundary
	constexpr uint64_t TotalItems = 4'000'000;

	bool runOne(const char* const _name, const bool _jitterProducer, const bool _jitterConsumer)
	{
		dsp56k::RingBuffer<uint64_t, Capacity, true> rb;

		std::atomic<uint64_t> produced{0};
		std::atomic<uint64_t> consumed{0};
		std::atomic<bool>     failed{false};
		std::atomic<bool>     done{false};

		const auto tStart = std::chrono::steady_clock::now();

		std::thread producer([&]
		{
			std::mt19937 rng(1);
			std::uniform_int_distribution<int> d(0, 8192);
			for(uint64_t i = 0; i < TotalItems && !failed.load(); ++i)
			{
				if(_jitterProducer && d(rng) == 0)
					std::this_thread::sleep_for(std::chrono::microseconds(1));
				rb.push_back(i);
				produced.fetch_add(1, std::memory_order_relaxed);
			}
		});

		std::thread consumer([&]
		{
			std::mt19937 rng(2);
			std::uniform_int_distribution<int> d(0, 8192);
			for(uint64_t i = 0; i < TotalItems; ++i)
			{
				if(_jitterConsumer && d(rng) == 0)
					std::this_thread::sleep_for(std::chrono::microseconds(1));
				const uint64_t v = rb.pop_front();
				if(v != i)
				{
					std::cerr << "  " << _name << ": MISMATCH at " << i << " got " << v << std::endl;
					failed.store(true);
					return;
				}
				consumed.fetch_add(1, std::memory_order_relaxed);
			}
		});

		// A lost wakeup hangs push_back/pop_front, which cannot be joined from here - so on a stall we
		// report and hard-exit the process. 10s with zero progress on a ring this fast is unambiguous.
		std::thread watchdog([&]
		{
			uint64_t last = 0;
			int stalled = 0;
			while(!done.load() && !failed.load())
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
				const uint64_t c = consumed.load(std::memory_order_relaxed);
				if(c == last && c < TotalItems)
				{
					if(++stalled >= 10)
					{
						std::cerr << "  " << _name << ": DEADLOCK - no progress for 10s (consumed=" << c
						          << " produced=" << produced.load() << "). Lost wakeup in ConditionVariable."
						          << std::endl;
						std::cerr.flush();
						std::_Exit(2);
					}
				}
				else
				{
					stalled = 0;
					last = c;
				}
			}
		});

		producer.join();
		consumer.join();
		done.store(true);
		watchdog.join();

		if(failed.load())
			return false;

		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - tStart).count();
		std::cout << "  " << _name << ": " << TotalItems << " items ok in " << ms << " ms" << std::endl;
		return true;
	}
}

int main()
{
	std::cout << "RingBuffer / ConditionVariable concurrency test (capacity " << Capacity << ")" << std::endl;

	bool ok = true;
	ok = ok && runOne("consumer-slower", false, true);
	ok = ok && runOne("producer-slower", true,  false);
	ok = ok && runOne("both-jittered",   true,  true);
	ok = ok && runOne("full-throttle",   false, false);	// pure ping-pong: the boot scenario

	if(!ok)
	{
		std::cerr << "FAILED" << std::endl;
		return 1;
	}

	std::cout << "PASSED" << std::endl;
	return 0;
}
