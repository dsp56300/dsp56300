#include "ringbuffer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>

// Regression test for the cross-thread SPSC usage of RingBuffer<T,C,false> (the "lock-free"
// spin/yield variant selected on all non-ARM64 builds, see dsp.h's m_pendingInterrupts /
// m_pendingExternalInterrupts). Before the fix, m_writeCount/m_readCount were plain non-atomic
// size_t: a producer thread and a consumer thread read/wrote them with no synchronization at
// all, which is a data race (UB) and, concretely, let an optimizing compiler treat empty()/
// full() as loop-invariant inside waitNotEmpty()/waitNotFull() - i.e. it could hoist the read
// out of the spin loop and never observe the other thread's write, hanging forever. This test
// drives exactly that cross-thread producer/consumer pattern under real OS thread scheduling
// with a watchdog to fail loudly instead of hanging CI if the race ever regresses.
//
// On the pre-fix code, this test doesn't just hang: it can observe outright wrong data (the
// consumer reading a value the producer never wrote at that position), after which a thread can
// end up permanently stuck inside RingBuffer's own waitNotEmpty()/waitNotFull() spin loop - so
// failure paths below call std::_Exit() directly rather than trying to join those threads.

static constexpr uint32_t BufferCapacity = 64;
static constexpr uint64_t TotalItems = 1'000'000;

int main()
{
	dsp56k::RingBuffer<uint64_t, BufferCapacity, false> ringBuffer;

	std::atomic<bool> done{false};
	std::atomic<uint64_t> produced{0};
	std::atomic<uint64_t> consumed{0};

	std::thread producer([&]
	{
		std::mt19937 rng(123);
		std::uniform_int_distribution<int> sleepDist(0, 200);

		for (uint64_t i = 0; i < TotalItems; ++i)
		{
			if (sleepDist(rng) == 0)
				std::this_thread::sleep_for(std::chrono::microseconds(1));

			ringBuffer.waitNotFull();
			ringBuffer.push_back(i);
			produced.fetch_add(1, std::memory_order_relaxed);
		}
	});

	std::thread consumer([&]
	{
		std::mt19937 rng(456);
		std::uniform_int_distribution<int> sleepDist(0, 200);

		for (uint64_t i = 0; i < TotalItems; ++i)
		{
			if (sleepDist(rng) == 0)
				std::this_thread::sleep_for(std::chrono::microseconds(1));

			ringBuffer.waitNotEmpty();
			const auto value = ringBuffer.pop_front();

			if (value != i)
			{
				std::cerr << "Consumer: expected " << i << " but got " << value << std::endl;
				std::cerr << "FAILED" << std::endl;
				std::_Exit(1);
			}

			consumed.fetch_add(1, std::memory_order_relaxed);
		}
	});

	std::thread watchdog([&]
	{
		uint64_t lastProduced = 0;
		uint64_t lastConsumed = 0;
		int stalledSeconds = 0;

		while (!done.load())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));

			const auto p = produced.load(std::memory_order_relaxed);
			const auto c = consumed.load(std::memory_order_relaxed);

			const auto progressed = (p != lastProduced) || (c != lastConsumed);
			lastProduced = p;
			lastConsumed = c;

			if (!progressed)
			{
				++stalledSeconds;
				if (stalledSeconds >= 5)
				{
					std::cerr << "DEADLOCK: no progress for 5 seconds (produced=" << p << ", consumed=" << c << ")" << std::endl;
					std::cerr << "FAILED" << std::endl;
					std::_Exit(1);
				}
			}
			else
			{
				stalledSeconds = 0;
			}
		}
	});

	producer.join();
	consumer.join();
	done.store(true);
	watchdog.join();

	if (consumed.load() != TotalItems)
	{
		std::cerr << "FAILED: wrong count, consumed=" << consumed.load() << " expected=" << TotalItems << std::endl;
		return 1;
	}

	std::cout << "PASSED" << std::endl;
	return 0;
}
