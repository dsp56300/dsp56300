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
	std::atomic<bool> done{false};
	std::atomic<uint64_t> totalConsumed{0};
	std::atomic<uint64_t> produced{0};
	std::array<std::atomic<uint64_t>, ConsumerCount> consumed{};

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

				consumed[c].fetch_add(1, std::memory_order_relaxed);
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

			if(sleepDist(rng) == 0)
				std::this_thread::sleep_for(std::chrono::microseconds(1));

			TestFrame frame;
			frame.value = i;
			buffer.push(frame);
			produced.fetch_add(1, std::memory_order_relaxed);
		}
	});

	// Watchdog thread
	std::thread watchdog([&]
	{
		uint64_t lastProduced = 0;
		std::array<uint64_t, ConsumerCount> lastConsumed{};
		int stalledSeconds = 0;

		while(!done.load() && !failed.load())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));

			const auto p = produced.load(std::memory_order_relaxed);
			bool anyProgress = (p != lastProduced);

			std::cerr << "  produced=" << p;
			for(uint32_t c = 0; c < ConsumerCount; ++c)
			{
				const auto cc = consumed[c].load(std::memory_order_relaxed);
				if(cc != lastConsumed[c])
					anyProgress = true;
				std::cerr << " c" << c << "=" << cc;
				lastConsumed[c] = cc;
			}
			std::cerr << " writeSem=" << buffer.writeCount() - buffer.readCount(0);
			for(uint32_t c = 0; c < ConsumerCount; ++c)
				std::cerr << " r" << c << "=" << buffer.readCount(c);
			std::cerr << std::endl;

			lastProduced = p;

			if(!anyProgress)
			{
				++stalledSeconds;
				if(stalledSeconds >= 5)
				{
					std::cerr << "DEADLOCK: no progress for 5 seconds" << std::endl;
					failed.store(true);
					buffer.terminate();
					return;
				}
			}
			else
			{
				stalledSeconds = 0;
			}
		}
	});

	producer.join();
	for(auto& t : consumers)
		t.join();
	done.store(true);
	watchdog.join();

	const auto expected = TotalFrames * ConsumerCount;
	const auto actual = totalConsumed.load();

	std::cout << "Total consumed: " << actual << " / " << expected << std::endl;

	if(failed.load())
	{
		std::cerr << "FAILED" << std::endl;
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
