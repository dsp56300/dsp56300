#include "sharedaudioreducer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
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

// Performance mode, "sharedAudioReducerTest perf": what the reducer itself costs per frame, headless on any platform.
// Producers that only add frames measure the synchronisation alone; producers that spin for a few microseconds per
// frame, with jitter, behave like DSP threads and show how much the reducer adds to their work
namespace perf
{
	using Frame = std::array<int32_t, 4>;

	struct Reduce
	{
		void operator()(Frame& _dst, const Frame& _src) const
		{
			for(size_t i = 0; i < _dst.size(); ++i)
				_dst[i] += _src[i];
		}
	};

	void spin(const std::chrono::nanoseconds _duration)
	{
		const auto end = std::chrono::steady_clock::now() + _duration;
		while(std::chrono::steady_clock::now() < end)
		{
		}
	}

	// prints the wall time per frame, all producers running in parallel
	template<uint32_t Producers>
	bool run(const uint64_t _frames, const uint32_t _workNs)
	{
		dsp56k::SharedAudioReducer<Frame, BufferCapacity, Producers, Reduce> reducer;

		std::array<uint32_t, Producers> ids{};
		for(auto& id : ids)
			id = reducer.addProducer();

		std::atomic<int64_t> sum{0};
		reducer.setCompletionCallback([&](uint64_t, const Frame& _frame)
		{
			sum.fetch_add(_frame[0], std::memory_order_relaxed);
		});

		std::vector<std::thread> threads;
		const auto start = std::chrono::steady_clock::now();

		for(uint32_t p = 0; p < Producers; ++p)
		{
			threads.emplace_back([&, p]
			{
				std::mt19937 rng(42 + p);
				std::uniform_int_distribution<uint32_t> jitter(0, _workNs / 2);

				for(uint64_t i = 0; i < _frames; ++i)
				{
					if(_workNs)
						spin(std::chrono::nanoseconds(_workNs * 3 / 4 + jitter(rng)));
					reducer.addFrame(ids[p], Frame{1, 0, 0, 0});
				}
			});
		}

		for(auto& t : threads)
			t.join();

		const auto ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / static_cast<double>(_frames);

		std::cout << "perf: producers " << Producers << ", work " << _workNs << " ns per frame: " << ns << " ns per frame";
		if(_workNs)
			std::cout << ", " << (ns / _workNs - 1.0) * 100.0 << " % above the work";
		std::cout << std::endl;

		if(sum.load() != static_cast<int64_t>(_frames * Producers))
		{
			std::cerr << "perf: FAILED, sum " << sum.load() << " instead of " << _frames * Producers << std::endl;
			return false;
		}
		return true;
	}

	int main()
	{
		std::cout << "perf: " << std::thread::hardware_concurrency() << " hardware threads, capacity " << BufferCapacity << std::endl;

		for(int rep = 0; rep < 3; ++rep)
		{
			if(!run<2>(2'000'000, 0) || !run<2>(300'000, 3000) || !run<9>(500'000, 0) || !run<9>(100'000, 3000))
				return 1;
		}
		return 0;
	}
}

int main(int _argc, char* _argv[])
{
	if(_argc > 1 && std::string(_argv[1]) == "perf")
		return perf::main();

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
