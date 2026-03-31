#include "sharedaudioreducer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

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
static constexpr uint64_t TotalFrames = 500;

int main()
{
	dsp56k::SharedAudioReducer<TestFrame, BufferCapacity, ProducerCount> reducer;

	std::vector<uint32_t> producerIds(ProducerCount);
	for(uint32_t i = 0; i < ProducerCount; ++i)
		producerIds[i] = reducer.addProducer();

	std::atomic<bool> failed{false};
	std::atomic<uint64_t> consumed{0};

	const uint64_t expectedIdSum = ProducerCount * (ProducerCount + 1) / 2;

	// Completion callback verifies the summed frame
	reducer.setCompletionCallback([&](uint64_t _frameIndex, const TestFrame& _frame)
	{
		const auto i = _frameIndex;

		const auto expectedFrameSum = i * ProducerCount;
		if(_frame[0][0] != expectedFrameSum)
		{
			std::cerr << "Frame " << i << " slot[0][0]: expected " << expectedFrameSum
			          << " got " << _frame[0][0] << std::endl;
			failed.store(true);
			return;
		}

		if(_frame[0][1] != expectedIdSum)
		{
			std::cerr << "Frame " << i << " slot[0][1]: expected " << expectedIdSum
			          << " got " << _frame[0][1] << std::endl;
			failed.store(true);
			return;
		}

		const auto expectedWeightedSum = i * expectedIdSum;
		if(_frame[1][0] != expectedWeightedSum)
		{
			std::cerr << "Frame " << i << " slot[1][0]: expected " << expectedWeightedSum
			          << " got " << _frame[1][0] << std::endl;
			failed.store(true);
			return;
		}

		consumed.fetch_add(1, std::memory_order_relaxed);
	});

	// Producer threads
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
				frame[0][0] = i;
				frame[0][1] = p + 1;
				frame[1][0] = i * (p + 1);
				reducer.addFrame(producerIds[p], frame);
			}
		});
	}

	for(auto& t : producers)
		t.join();

	if(failed.load())
	{
		std::cerr << "FAILED" << std::endl;
		return 1;
	}

	if(consumed.load() != TotalFrames)
	{
		std::cerr << "FAILED: consumed " << consumed.load() << " / " << TotalFrames << std::endl;
		return 1;
	}

	std::cout << "PASSED" << std::endl;
	return 0;
}
