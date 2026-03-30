#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace dsp56k
{
	// Single-producer, multi-consumer ring buffer for audio frames.
	//
	// One producer writes frames via push(). Multiple consumers (DSPs)
	// each read the same sequence of frames via pop() with their own
	// read position. Each consumer sees every frame exactly once.
	//
	// Blocking:
	//   - pop() blocks if no unread frames are available for that consumer
	//   - push() blocks if the ring buffer is full, i.e. the slowest
	//     consumer has not yet read the oldest frame
	//
	// All consumers share a single condition_variable for read blocking.
	// One push() triggers one notify_all() call, waking all blocked
	// consumers simultaneously.
	template<typename TFrame, uint32_t Capacity, uint32_t MaxConsumers = 16>
	class SharedAudioBuffer
	{
	public:
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

		SharedAudioBuffer() = default;

		// Wake all blocked consumers and producer so they can exit.
		void terminate()
		{
			m_terminated = true;
			m_readMutex.lock();
			m_readMutex.unlock();
			m_readCv.notify_all();
			m_writeMutex.lock();
			m_writeMutex.unlock();
			m_writeCv.notify_one();
		}

		bool isTerminated() const { return m_terminated; }

		// Register a new consumer. Returns a consumer index used for pop().
		// Must be called before any push/pop. Not thread-safe with push/pop.
		uint32_t addConsumer()
		{
			const auto idx = m_consumerCount++;
			assert(idx < MaxConsumers);
			m_readCounts[idx].store(0, std::memory_order_relaxed);
			return idx;
		}

		// Write a frame. Blocks if the slowest consumer hasn't read yet.
		void push(const TFrame& _frame)
		{
			// Wait until all consumers have read enough to make room
			if(m_writeCount - slowestReadCount() >= Capacity)
			{
				std::unique_lock lock(m_writeMutex);
				m_writeCv.wait(lock, [this]
				{
					return m_terminated || m_writeCount - slowestReadCount() < Capacity;
				});
				if(m_terminated)
					return;
			}

			m_data[wrap(m_writeCount)] = _frame;
			++m_writeCount;

			// Wake all consumers with a single notify_all.
			// Lock/unlock ensures no consumer is between predicate check
			// and entering wait, preventing lost notifications.
			m_readMutex.lock();
			m_readMutex.unlock();
			m_readCv.notify_all();
		}

		// Read the next frame for consumer _index. Blocks if no data available.
		TFrame pop(const uint32_t _index)
		{
			auto& readCount = m_readCounts[_index];
			auto rc = readCount.load(std::memory_order_relaxed);

			// Wait if no data available for this consumer
			if(rc >= m_writeCount)
			{
				std::unique_lock lock(m_readMutex);
				m_readCv.wait(lock, [&]
				{
					return m_terminated || readCount.load(std::memory_order_relaxed) < m_writeCount;
				});
				if(m_terminated)
					return {};
				rc = readCount.load(std::memory_order_relaxed);
			}

			const TFrame result = m_data[wrap(rc)];
			readCount.store(rc + 1, std::memory_order_relaxed);

			// Notify producer in case it's blocked waiting for space
			m_writeMutex.lock();
			m_writeMutex.unlock();
			m_writeCv.notify_one();

			return result;
		}

		// Non-blocking check if consumer _index has data available
		bool hasData(const uint32_t _index) const
		{
			return m_readCounts[_index].load(std::memory_order_relaxed) < m_writeCount;
		}

		uint64_t writeCount() const { return m_writeCount.load(std::memory_order_relaxed); }
		uint64_t readCount(const uint32_t _index) const { return m_readCounts[_index].load(std::memory_order_relaxed); }
		uint32_t consumerCount() const { return m_consumerCount; }

	private:
		static constexpr uint64_t wrap(const uint64_t _count)
		{
			return _count & (Capacity - 1);
		}

		uint64_t slowestReadCount() const
		{
			auto minRC = m_writeCount.load(std::memory_order_relaxed);
			for(uint32_t i = 0; i < m_consumerCount; ++i)
			{
				const auto rc = m_readCounts[i].load(std::memory_order_relaxed);
				if(rc < minRC)
					minRC = rc;
			}
			return minRC;
		}

		std::array<TFrame, Capacity> m_data;

		std::atomic<uint64_t> m_writeCount{0};
		bool m_terminated = false;
		uint32_t m_consumerCount = 0;
		std::array<std::atomic<uint64_t>, MaxConsumers> m_readCounts{};

		// Read side: single CV shared by all consumers
		std::mutex m_readMutex;
		std::condition_variable m_readCv;

		// Write side: backpressure from slowest consumer
		std::mutex m_writeMutex;
		std::condition_variable m_writeCv;
	};
}
