#include "sharedaudioreducer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

// Simple frame type matching the interface: operator[], .size(), .resize()
struct TestFrame
{
	struct Slot
	{
		uint64_t values[2] = {0, 0};
		uint64_t& operator[](size_t i) { return values[i]; }
		const uint64_t& operator[](size_t i) const { return values[i]; }
		size_t size() const { return 2; }
	};

	Slot slots[4] = {};
	uint32_t slotCount = 0;

	Slot& operator[](size_t i) { return slots[i]; }
	const Slot& operator[](size_t i) const { return slots[i]; }
	uint32_t size() const { return slotCount; }
	void resize(uint32_t s) { slotCount = s; }
};

static constexpr uint32_t BufferCapacity = 64;
static constexpr uint32_t ProducerCount = 6;
static constexpr uint64_t TotalFrames = 1'000'000;

int main()
{
	dsp56k::SharedAudioReducer<TestFrame, BufferCapacity, ProducerCount> reducer;

	std::vector<uint32_t> producerIds(ProducerCount);
	for(uint32_t i = 0; i < ProducerCount; ++i)
		producerIds[i] = reducer.addProducer();

	std::atomic<bool> failed{false};

	// Producer threads: each writes frame number as value in slot 0, channel 0
	// and its own producer ID in slot 0, channel 1
	std::vector<std::thread> producers;
	producers.reserve(ProducerCount);

	for(uint32_t p = 0; p < ProducerCount; ++p)
	{
		producers.emplace_back([&, p]
		{
			std::mt19937 rng(42 + p);
			std::uniform_int_distribution<int> sleepDist(0, 200);

			for(uint64_t i = 0; i < TotalFrames; ++i)
			{
				if(failed.load())
					return;

				if(sleepDist(rng) == 0)
					std::this_thread::sleep_for(std::chrono::microseconds(1));

				TestFrame frame;
				frame.resize(4);
				frame[0][0] = i;          // frame index — should sum to i * ProducerCount
				frame[0][1] = p + 1;      // producer ID+1 — should sum to 1+2+3+4+5+6 = 21
				frame[1][0] = i * (p + 1); // unique per producer — verify sum
				reducer.addFrame(producerIds[p], frame);
			}
		});
	}

	// Consumer thread
	std::thread consumer([&]
	{
		const uint64_t expectedIdSum = ProducerCount * (ProducerCount + 1) / 2;

		for(uint64_t i = 0; i < TotalFrames; ++i)
		{
			if(failed.load())
				return;

			const auto frame = reducer.pop();

			// Check slot 0 channel 0: sum of frame indices = i * ProducerCount
			const auto expectedFrameSum = i * ProducerCount;
			if(frame[0][0] != expectedFrameSum)
			{
				std::cerr << "Frame " << i << " slot[0][0]: expected " << expectedFrameSum
				          << " got " << frame[0][0] << std::endl;
				failed.store(true);
				return;
			}

			// Check slot 0 channel 1: sum of (p+1) = 1+2+3+4+5+6 = 21
			if(frame[0][1] != expectedIdSum)
			{
				std::cerr << "Frame " << i << " slot[0][1]: expected " << expectedIdSum
				          << " got " << frame[0][1] << std::endl;
				failed.store(true);
				return;
			}

			// Check slot 1 channel 0: sum of i*(p+1) for p=0..5 = i * (1+2+3+4+5+6) = i * 21
			const auto expectedWeightedSum = i * expectedIdSum;
			if(frame[1][0] != expectedWeightedSum)
			{
				std::cerr << "Frame " << i << " slot[1][0]: expected " << expectedWeightedSum
				          << " got " << frame[1][0] << std::endl;
				failed.store(true);
				return;
			}

			if((i & 0xFFFFF) == 0 && i > 0)
				std::cout << "  Consumed " << i << " / " << TotalFrames << std::endl;
		}
	});

	for(auto& t : producers)
		t.join();
	consumer.join();

	if(failed.load())
	{
		std::cerr << "FAILED" << std::endl;
		return 1;
	}

	std::cout << "PASSED" << std::endl;
	return 0;
}
