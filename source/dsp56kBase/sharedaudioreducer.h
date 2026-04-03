#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>

#include "conditionvariable.h"

namespace dsp56k
{
	// Multi-producer ring buffer that sums frames from all producers.
	//
	// Lock-free producer path: each producer writes to its own lane.
	// The last producer to arrive does the reduction (summation) and
	// calls the completion callback.
	//
	// Backpressure uses a condition variable with a fast-path atomic
	// check — the mutex is only taken when producers are actually
	// blocked (buffer full), which is rare in steady-state operation.
	template<typename TFrame, uint32_t Capacity, uint32_t MaxProducers, typename TReduceFunc>
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

		void addFrame(const uint32_t _index, const TFrame& _frame)
		{
			if(m_terminated)
				return;

			auto& writeCount = m_writeCounts[_index];
			const auto wc = writeCount.load(std::memory_order_relaxed);

			// Fast path: no backpressure, skip mutex entirely
			if(wc - m_readCount.load(std::memory_order_acquire) >= Capacity)
			{
				std::unique_lock<std::mutex> lock(m_readMtx);
				m_readCv.wait(lock, [&]()
				{
					return m_terminated || wc - m_readCount.load(std::memory_order_acquire) < Capacity;
				});
				if(m_terminated)
					return;
			}

			auto& slot = m_data[wrap(wc)];

			slot.lanes[_index] = _frame;

			std::atomic_thread_fence(std::memory_order_release);

			writeCount.store(wc + 1, std::memory_order_release);

			const auto contrib = slot.contributions.fetch_add(1, std::memory_order_acq_rel) + 1;
			if(contrib == m_producerCount)
			{
				TFrame result = slot.lanes[0];

				for(uint32_t p = 1; p < m_producerCount; ++p)
					m_reduceFunc(result, slot.lanes[p]);

				m_completionCallback(wc, result);

				slot.contributions.store(0, std::memory_order_relaxed);
				m_readCount.store(wc + 1, std::memory_order_release);

				{
					std::lock_guard<std::mutex> lock(m_readMtx);
				}
				m_readCv.notify_all();
			}
		}

		void terminate()
		{
			m_terminated = true;
			{
				std::lock_guard<std::mutex> lock(m_readMtx);
			}
			m_readCv.notify_all();
		}

		bool isTerminated() const { return m_terminated; }
		uint32_t producerCount() const { return m_producerCount; }
		uint64_t readCount() const { return m_readCount.load(std::memory_order_relaxed); }

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

		std::mutex m_readMtx;
		ConditionVariable m_readCv;

		CompletionCallback m_completionCallback;
		TReduceFunc m_reduceFunc;
	};
}
