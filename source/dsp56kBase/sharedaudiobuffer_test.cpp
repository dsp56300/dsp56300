#include "sharedaudiobuffer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

// Simple frame type: just holds one uint64_t value
struct TestFrame
{
	uint64_t value = 0;
};

static constexpr uint32_t BufferCapacity = 64;
static constexpr uint32_t ConsumerCount = 6;
static constexpr uint64_t TotalFrames = 1'000'000;

int main()
{
	dsp56k::SharedAudioBuffer<TestFrame, BufferCapacity, ConsumerCount> buffer;

	std::vector<uint32_t> consumerIds(ConsumerCount);
	for(uint32_t i = 0; i < ConsumerCount; ++i)
		consumerIds[i] = buffer.addConsumer();

	std::atomic<bool> failed{false};
	std::atomic<uint64_t> totalConsumed{0};

	// Consumer threads
	std::vector<std::thread> consumers;
	consumers.reserve(ConsumerCount);

	for(uint32_t c = 0; c < ConsumerCount; ++c)
	{
		consumers.emplace_back([&, c]
		{
			std::mt19937 rng(42 + c);
			std::uniform_int_distribution<int> sleepDist(0, 200);

			for(uint64_t i = 0; i < TotalFrames; ++i)
			{
				// Randomly sleep to create contention
				if(sleepDist(rng) == 0)
					std::this_thread::sleep_for(std::chrono::microseconds(1));

				const auto frame = buffer.pop(consumerIds[c]);

				if(frame.value != i)
				{
					std::cerr << "Consumer " << c << ": expected " << i
					          << " but got " << frame.value << std::endl;
					failed.store(true);
					return;
				}

				totalConsumed.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	// Producer thread
	std::thread producer([&]
	{
		std::mt19937 rng(123);
		std::uniform_int_distribution<int> sleepDist(0, 200);

		for(uint64_t i = 0; i < TotalFrames; ++i)
		{
			if(failed.load())
				return;

			// Randomly sleep to let consumers catch up / drain buffer
			if(sleepDist(rng) == 0)
				std::this_thread::sleep_for(std::chrono::microseconds(1));

			TestFrame frame;
			frame.value = i;
			buffer.push(frame);

			if((i & 0xFFFFF) == 0 && i > 0)
				std::cout << "  Produced " << i << " / " << TotalFrames << std::endl;
		}
	});

	producer.join();
	for(auto& t : consumers)
		t.join();

	const auto expected = TotalFrames * ConsumerCount;
	const auto actual = totalConsumed.load();

	std::cout << "Total consumed: " << actual << " / " << expected << std::endl;

	if(failed.load())
	{
		std::cerr << "FAILED: data corruption detected" << std::endl;
		return 1;
	}

	if(actual != expected)
	{
		std::cerr << "FAILED: wrong count" << std::endl;
		return 1;
	}

	std::cout << "PASSED" << std::endl;
	return 0;
}
