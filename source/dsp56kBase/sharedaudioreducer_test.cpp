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

static constexpr uint32_t BufferCapacity = 16;	// what nova ships with; small enough that backpressure and slot adjacency actually engage
static constexpr uint32_t ProducerCount = 6;
static constexpr uint64_t TotalFrames = 50000;

struct DefaultReduce
{
	void operator()(TestFrame& _dst, const TestFrame& _src) const
	{
		const auto srcSize = _src.size();
		if(srcSize > _dst.size())
			_dst.resize(srcSize);
		for(uint32_t s = 0; s < srcSize; ++s)
			for(uint32_t c = 0; c < _src[s].size(); ++c)
				_dst[s][c] += _src[s][c];
	}
};

int main()
{
	dsp56k::SharedAudioReducer<TestFrame, BufferCapacity, ProducerCount, DefaultReduce> reducer;

	std::vector<uint32_t> producerIds(ProducerCount);
	for(uint32_t i = 0; i < ProducerCount; ++i)
		producerIds[i] = reducer.addProducer();

	std::atomic<bool> failed{false};
	std::atomic<bool> done{false};
	std::atomic<uint64_t> consumed{0};
	std::array<std::atomic<uint64_t>, ProducerCount> produced{};

	const uint64_t expectedIdSum = ProducerCount * (ProducerCount + 1) / 2;

	std::atomic<int> inCallback{0};
	std::atomic<uint64_t> nextExpectedFrame{0};

	reducer.setCompletionCallback([&](uint64_t _frameIndex, const TestFrame& _frame)
	{
		// Assert the CONTRACT, not just the sums. The production completion callbacks pop/push SPSC stage
		// buffers, so the reducer must run them serialized and in slot order - two overlapping completions
		// (the last contributor of slot S is still inside the callback while slot S+1 fills up and its last
		// contributor fires the next one) corrupt that accounting long before any data mismatch would show.
		// This is exactly the intermittent multi-instance pipeline deadlock; the previous version of this
		// test could not see it because its callback was concurrency-tolerant arithmetic.
		if(inCallback.fetch_add(1, std::memory_order_acq_rel) != 0)
		{
			std::cerr << "Frame " << _frameIndex << ": CONCURRENT completion callbacks" << std::endl;
			failed.store(true);
		}

		const auto expectedIndex = nextExpectedFrame.load(std::memory_order_relaxed);
		if(_frameIndex != expectedIndex)
		{
			std::cerr << "Frame " << _frameIndex << ": OUT-OF-ORDER completion, expected frame "
			          << expectedIndex << std::endl;
			failed.store(true);
		}
		nextExpectedFrame.store(_frameIndex + 1, std::memory_order_relaxed);

		// Dwell like the real callbacks do (they block on backpressured buffers): the overlap window IS the
		// callback duration, so a callback returning in nanoseconds hides the race the assert above flags.
		if((_frameIndex & 63) == 0)
			std::this_thread::sleep_for(std::chrono::microseconds(50));

		const auto i = _frameIndex;

		const auto expectedFrameSum = i * ProducerCount;
		if(_frame[0][0] != expectedFrameSum)
		{
			std::cerr << "Frame " << i << " slot[0][0]: expected " << expectedFrameSum
			          << " got " << _frame[0][0] << std::endl;
			failed.store(true);
		}

		if(_frame[0][1] != expectedIdSum)
		{
			std::cerr << "Frame " << i << " slot[0][1]: expected " << expectedIdSum
			          << " got " << _frame[0][1] << std::endl;
			failed.store(true);
		}

		if(!failed.load())
			consumed.fetch_add(1, std::memory_order_relaxed);

		inCallback.fetch_sub(1, std::memory_order_acq_rel);
	});

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
				produced[p].fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	// Watchdog
	std::thread watchdog([&]
	{
		uint64_t lastConsumed = 0;
		std::array<uint64_t, ProducerCount> lastProduced{};
		int stalledSeconds = 0;

		while(!done.load() && !failed.load())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));

			const auto c = consumed.load(std::memory_order_relaxed);
			bool anyProgress = (c != lastConsumed);

			std::cerr << "  consumed=" << c << " readCount=" << reducer.readCount();
			for(uint32_t p = 0; p < ProducerCount; ++p)
			{
				const auto pp = produced[p].load(std::memory_order_relaxed);
				if(pp != lastProduced[p])
					anyProgress = true;
				std::cerr << " p" << p << "=" << pp;
				lastProduced[p] = pp;
			}
			std::cerr << std::endl;

			lastConsumed = c;

			if(!anyProgress)
			{
				++stalledSeconds;
				if(stalledSeconds >= 5)
				{
					std::cerr << "DEADLOCK: no progress for 5 seconds" << std::endl;
					failed.store(true);
					reducer.terminate();
					return;
				}
			}
			else
			{
				stalledSeconds = 0;
			}
		}
	});

	for(auto& t : producers)
		t.join();
	done.store(true);
	watchdog.join();

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
