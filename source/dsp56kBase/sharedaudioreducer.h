#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>

#include "semaphore.h"

namespace dsp56k
{
	// Multi-producer, single-consumer ring buffer that sums frames.
	//
	// Lock-free producer path: each producer writes to its own lane.
	// The last producer to arrive does the reduction (summation) and
	// either calls the completion callback (auto-pop) or notifies the
	// consumer semaphore.
	//
	// No mutexes. No contention between producers on write.
	template<typename TFrame, uint32_t Capacity, uint32_t MaxProducers = 16>
	class SharedAudioReducer
	{
	public:
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

		using CompletionCallback = std::function<void(uint64_t, const TFrame&)>;

		SharedAudioReducer()
		{
			for(auto& slot : m_data)
			{
				slot.contributions.store(0, std::memory_order_relaxed);
				for(auto& lane : slot.lanes)
					::memset(&lane, 0, sizeof(lane));
			}
		}

		void setCompletionCallback(CompletionCallback _callback)
		{
			m_completionCallback = std::move(_callback);
		}

		uint32_t addProducer()
		{
			const auto idx = m_producerCount++;
			assert(idx < MaxProducers);
			m_writeCounts[idx].store(0, std::memory_order_relaxed);
			return idx;
		}

		// Lock-free: each producer writes to its own lane, no contention.
		void addFrame(const uint32_t _index, const TFrame& _frame)
		{
			auto& writeCount = m_writeCounts[_index];
			const auto wc = writeCount.load(std::memory_order_relaxed);

			// Block if Capacity frames ahead of consumer.
			// Spin-check first (fast path), fall back to semaphore.
			while(wc - m_readCount.load(std::memory_order_acquire) >= Capacity)
				m_readSem.wait();

			auto& slot = m_data[wrap(wc)];

			// Write to own lane — no lock, no contention
			slot.lanes[_index] = _frame;

			// ensure lane write is visible before contribution
			std::atomic_thread_fence(std::memory_order_release);

			writeCount.store(wc + 1, std::memory_order_release);

			// Last producer does the reduction into a local temp
			if(slot.contributions.fetch_add(1, std::memory_order_acq_rel) + 1 == m_producerCount)
			{
				// Sum all lanes into a temporary (not in-place — other
				// producers may already be writing to this slot for the
				// next frame after readCount advances)
				TFrame result = slot.lanes[0];

				for(uint32_t p = 1; p < m_producerCount; ++p)
				{
					const auto& src = slot.lanes[p];
					const auto srcSize = src.size();

					if(srcSize > result.size())
						result.resize(srcSize);

					for(uint32_t s = 0; s < srcSize; ++s)
						for(uint32_t c = 0; c < src[s].size(); ++c)
							result[s][c] += src[s][c];
				}

				m_completionCallback(wc, result);

				// Reset and advance AFTER sum+callback so producers
				// can't overwrite lanes while we're still reading them
				slot.contributions.store(0, std::memory_order_relaxed);
				m_readCount.store(wc + 1, std::memory_order_release);

				m_readSem.notifyAll(m_producerCount);
			}
		}

		void terminate()
		{
			m_terminated = true;
			m_readSem.notifyAll(MaxProducers);
		}

		bool isTerminated() const { return m_terminated; }
		uint32_t producerCount() const { return m_producerCount; }

	private:
		static constexpr uint64_t wrap(const uint64_t _count)
		{
			return _count & (Capacity - 1);
		}

		struct Slot
		{
			std::atomic<uint32_t> contributions{0};
			std::array<TFrame, MaxProducers> lanes{};
		};

		std::array<Slot, Capacity> m_data{};

		std::atomic<uint64_t> m_readCount{0};
		uint32_t m_producerCount = 0;
		bool m_terminated = false;
		std::array<std::atomic<uint64_t>, MaxProducers> m_writeCounts{};

		// Used as a wakeup signal, not a counter. Producers spin on m_readCount
		// and fall back to this semaphore when they need to block.
		Semaphore m_readSem{0};

		CompletionCallback m_completionCallback;
	};
}
