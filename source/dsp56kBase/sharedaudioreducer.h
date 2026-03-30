#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsp56k
{
	// Multi-producer, single-consumer ring buffer that sums frames.
	//
	// Multiple producers (DSPs) each write one frame per position via
	// addFrame(). When all producers have written to a position, the
	// summed frame becomes available to the consumer via pop().
	//
	// Blocking:
	//   - pop() blocks if no fully-summed frame is available yet
	//   - addFrame() blocks if the producer is too far ahead of the consumer
	//
	// Each position accumulates TxFrames from all producers. The consumer
	// receives the sum of all producers' contributions.
	template<typename TFrame, uint32_t Capacity, uint32_t MaxProducers = 16>
	class SharedAudioReducer
	{
	public:
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

		SharedAudioReducer() = default;

		// Register a new producer. Returns a producer index used for addFrame().
		// Must be called before any addFrame/pop. Not thread-safe with addFrame/pop.
		uint32_t addProducer()
		{
			const auto idx = m_producerCount++;
			assert(idx < MaxProducers);
			m_writeCounts[idx].store(0, std::memory_order_relaxed);
			return idx;
		}

		// Add a frame from producer _index. Accumulates (sums) into the
		// current write position. Blocks if too far ahead of the consumer.
		void addFrame(const uint32_t _index, const TFrame& _frame)
		{
			auto& writeCount = m_writeCounts[_index];
			const auto wc = writeCount.load(std::memory_order_relaxed);

			// Block if we're Capacity frames ahead of the consumer
			if(wc - m_readCount.load(std::memory_order_relaxed) >= Capacity)
			{
				std::unique_lock lock(m_readMutex);
				m_readCv.wait(lock, [&]
				{
					return m_terminated || wc - m_readCount.load(std::memory_order_relaxed) < Capacity;
				});
				if(m_terminated)
					return;
			}

			auto& slot = m_data[wrap(wc)];

			// Accumulate this producer's contribution under per-slot lock
			{
				std::lock_guard lock(slot.mutex);
				if(_frame.size() > slot.frame.size())
					slot.frame.resize(_frame.size());
				for(uint32_t s = 0; s < _frame.size(); ++s)
				{
					for(uint32_t c = 0; c < _frame[s].size(); ++c)
						slot.frame[s][c] += _frame[s][c];
				}
			}

			writeCount.store(wc + 1, std::memory_order_relaxed);

			// If this was the last producer to write to this position,
			// the summed frame is ready for the consumer
			if(slot.contributions.fetch_add(1, std::memory_order_acq_rel) + 1 == m_producerCount)
			{
				m_writeMutex.lock();
				m_writeMutex.unlock();
				m_writeCv.notify_one();
			}
		}

		// Read the next fully-summed frame. Blocks until all producers
		// have written to this position. Resets the slot for reuse.
		TFrame pop()
		{
			const auto rc = m_readCount.load(std::memory_order_relaxed);

			auto& slot = m_data[wrap(rc)];

			// Wait until all producers have contributed
			if(slot.contributions.load(std::memory_order_acquire) < m_producerCount)
			{
				std::unique_lock lock(m_writeMutex);
				m_writeCv.wait(lock, [&]
				{
					return m_terminated || slot.contributions.load(std::memory_order_acquire) >= m_producerCount;
				});
				if(m_terminated)
					return {};
			}

			const TFrame result = slot.frame;

			// Reset slot for reuse
			::memset(&slot.frame, 0, sizeof(slot.frame));
			slot.frame.resize(0);
			slot.contributions.store(0, std::memory_order_relaxed);

			m_readCount.store(rc + 1, std::memory_order_relaxed);

			// Notify producers in case they're blocked waiting for space
			m_readMutex.lock();
			m_readMutex.unlock();
			m_readCv.notify_all();

			return result;
		}

		void terminate()
		{
			m_terminated = true;
			m_writeMutex.lock();
			m_writeMutex.unlock();
			m_writeCv.notify_one();
			m_readMutex.lock();
			m_readMutex.unlock();
			m_readCv.notify_all();
		}

		bool isTerminated() const { return m_terminated; }

		uint32_t producerCount() const { return m_producerCount; }

	private:
		static constexpr uint64_t wrap(const uint64_t _count)
		{
			return _count & (Capacity - 1);
		}

		struct alignas(64) Slot
		{
			TFrame frame{};
			std::atomic<uint32_t> contributions{0};
			std::mutex mutex;
		};

		std::array<Slot, Capacity> m_data{};

		std::atomic<uint64_t> m_readCount{0};
		uint32_t m_producerCount = 0;
		bool m_terminated = false;
		std::array<std::atomic<uint64_t>, MaxProducers> m_writeCounts{};

		// Consumer waits here for all producers to finish a slot
		std::mutex m_writeMutex;
		std::condition_variable m_writeCv;

		// Producers wait here for consumer to free space
		std::mutex m_readMutex;
		std::condition_variable m_readCv;
	};
}
