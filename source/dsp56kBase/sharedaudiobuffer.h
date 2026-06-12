#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>

#include "semaphore.h"

namespace dsp56k
{
	// Single-producer, multi-consumer ring buffer for audio frames.
	//
	// Read side: per-consumer SpscSemaphoreWithCount (atomic fast path).
	// Write side: per-slot completion tracking, SpscSemaphoreWithCount
	// for backpressure. A slot is freed when ALL consumers have read it.
	template<typename TFrame, uint32_t Capacity, uint32_t MaxConsumers = 16>
	class SharedAudioBuffer
	{
	public:
		static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

		SharedAudioBuffer() = default;

		uint32_t addConsumer()
		{
			const auto idx = m_consumerCount++;
			assert(idx < MaxConsumers);
			return idx;
		}

		void push(const TFrame& _frame)
		{
			if(m_terminated)
				return;

			// Block if buffer full (atomic fast path via semaphore)
			m_writeSem.wait();

			if(m_terminated)
				return;

			const auto wc = m_writeCount.load(std::memory_order_relaxed);
			auto& slot = m_slots[wrap(wc)];
			slot.frame = _frame;
			m_writeCount.store(wc + 1, std::memory_order_release);

			// Wake all consumers (atomic fast path per consumer)
			for(uint32_t i = 0; i < m_consumerCount; ++i)
				m_readSems[i].notify();
		}

		TFrame pop(const uint32_t _index)
		{
			if(m_terminated)
				return {};

			// Atomic fast path: returns immediately if data available
			m_readSems[_index].wait();

			if(m_terminated)
				return {};

			const auto rc = m_readCounts[_index].load(std::memory_order_relaxed);
			auto& slot = m_slots[wrap(rc)];
			const TFrame result = slot.frame;
			m_readCounts[_index].store(rc + 1, std::memory_order_release);

			// If all consumers have read this slot, free it for the producer.
			// Uses modular check: readCount accumulates across cycles,
			// slot is freed every consumerCount increments.
			if((slot.readCount.fetch_add(1, std::memory_order_acq_rel) + 1) % m_consumerCount == 0)
				m_writeSem.notify();

			return result;
		}

		void terminate()
		{
			m_terminated = true;

			// Wake any thread currently blocked in a semaphore wait.
			// Threads check m_terminated before (re-)entering wait.
			for(uint32_t i = 0; i < m_consumerCount; ++i)
				m_readSems[i].notify();
			m_writeSem.notify();
		}

		bool isTerminated() const { return m_terminated; }
		uint64_t writeCount() const { return m_writeCount.load(std::memory_order_relaxed); }
		uint64_t readCount(const uint32_t _index) const { return m_readCounts[_index].load(std::memory_order_relaxed); }
		uint32_t consumerCount() const { return m_consumerCount; }

	private:
		static constexpr uint64_t wrap(const uint64_t _count)
		{
			return _count & (Capacity - 1);
		}

		struct Slot
		{
			TFrame frame{};
			std::atomic<uint32_t> readCount{0};
		};

		std::array<Slot, Capacity> m_slots{};

		std::atomic<uint64_t> m_writeCount{0};
		bool m_terminated = false;

		uint32_t m_consumerCount = 0;
		std::array<std::atomic<uint64_t>, MaxConsumers> m_readCounts{};

		// Per-consumer read semaphores (atomic fast path)
		std::array<SpscSemaphoreWithCount, MaxConsumers> m_readSems{};

		// Write backpressure: starts at Capacity, decremented per push,
		// incremented when ALL consumers have read a slot
		SpscSemaphoreWithCount m_writeSem{static_cast<int>(Capacity)};
	};
}
