#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>

#include "semaphore.h"

namespace dsp56k
{
	// Multi-producer, single-consumer ring buffer that sums frames.
	//
	// Multiple producers (DSPs) each write one frame per position via
	// addFrame(). When all producers have written to a position, the
	// summed frame becomes available to the consumer via pop().
	//
	// Uses atomic counters for the fast path. Per-slot mutex protects
	// the accumulation. Only blocks via Semaphore when actually full/empty.
	template<typename TFrame, uint32_t Capacity, uint32_t MaxProducers = 16>
	class SharedAudioReducer
	{
	public:
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

		using CompletionCallback = std::function<void(uint64_t, const TFrame&)>;

		SharedAudioReducer() = default;

		void setCompletionCallback(CompletionCallback _callback)
		{
			m_completionCallback = std::move(_callback);
		}

		// Register a new producer. Returns a producer index.
		uint32_t addProducer()
		{
			const auto idx = m_producerCount++;
			assert(idx < MaxProducers);
			m_writeCounts[idx].store(0, std::memory_order_relaxed);
			return idx;
		}

		// Add a frame from producer _index. Blocks only if too far ahead.
		void addFrame(const uint32_t _index, const TFrame& _frame)
		{
			auto& writeCount = m_writeCounts[_index];
			const auto wc = writeCount.load(std::memory_order_relaxed);

			// Block if Capacity frames ahead of consumer
			if(wc - m_readCount.load(std::memory_order_relaxed) >= Capacity)
				m_readSem.wait();

			auto& slot = m_data[wrap(wc)];

			// Accumulate under per-slot lock
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

			// If this was the last producer, the frame is complete
			if(slot.contributions.fetch_add(1, std::memory_order_acq_rel) + 1 == m_producerCount)
			{
				if(m_completionCallback)
				{
					m_completionCallback(wc, slot.frame);

					// Auto-pop: reset slot and advance read count
					::memset(&slot.frame, 0, sizeof(slot.frame));
					slot.frame.resize(0);
					slot.contributions.store(0, std::memory_order_relaxed);
					m_readCount.store(wc + 1, std::memory_order_release);

					// Notify producers waiting for space
					m_readSem.notify();
				}
				else
				{
					// Notify pop() consumer
					m_writeSem.notify();
				}
			}
		}

		// Read the next fully-summed frame. Blocks until all producers done.
		TFrame pop()
		{
			m_writeSem.wait();

			const auto rc = m_readCount.load(std::memory_order_relaxed);
			auto& slot = m_data[wrap(rc)];

			const TFrame result = slot.frame;

			// Reset slot
			::memset(&slot.frame, 0, sizeof(slot.frame));
			slot.frame.resize(0);
			slot.contributions.store(0, std::memory_order_relaxed);

			m_readCount.store(rc + 1, std::memory_order_release);

			// Notify producers waiting for space
			m_readSem.notify();

			return result;
		}

		void terminate()
		{
			m_terminated = true;
			m_writeSem.notify();
			m_readSem.notify();
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

		// Consumer waits here for completed frames (when no completion callback)
		SpscSemaphoreWithCount m_writeSem{0};

		// Producers wait here for consumer to free space
		SpscSemaphoreWithCount m_readSem{static_cast<int>(Capacity)};

		CompletionCallback m_completionCallback;
	};
}
