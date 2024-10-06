#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>

namespace dsp56k
{
	class Semaphore
	{
	public:
		explicit Semaphore (const uint32_t _count = 0) : m_count(_count) 
	    {
	    }
	    
	    void notify(const uint32_t _count = 1)
		{
			{
		        Lock lock(m_mutex);
		        m_count += _count;
			}

			//notify the waiting thread
	        m_cv.notify_one();
	    }

	    void wait(const uint32_t _count = 1)
		{
	        Lock lock(m_mutex);

            // wait on the mutex until notify is called
			m_cv.wait(lock, [&]()
			{
				return m_count >= _count;
			});

			m_count -= _count;
	    }
	private:
		using Lock = std::unique_lock<std::mutex>;
	    std::mutex m_mutex;
	    std::condition_variable m_cv;
	    uint32_t m_count;
	};

	class SpscSemaphore
	{
	public:
		explicit SpscSemaphore(const int _count = 0) : m_count(_count)
		{
		}

		void notify()
		{
			const auto rlx = std::memory_order_relaxed;
			const auto acqRel = std::memory_order_acq_rel;

			const auto prev = m_count.fetch_add(1, acqRel);
			if (prev >= 0)
			{
				// no one waiting
				return;
			}

			Lock lock(m_mutex);
			m_cv.notify_one();
		}

		void wait()
		{
			const auto rlx = std::memory_order_relaxed;
			const auto acqRel = std::memory_order_acq_rel;

			const auto prev = m_count.fetch_sub(1, acqRel);
			const auto now = prev - 1;
			if (prev > 0) 
			{
				// data available
				return;
			}
			auto actual = now;
			Lock lock(m_mutex);
			// Check if "m_count" is still unmodified. If it doesn't it means that the
			// only producer (this algo is SPSC only) was able to see our change before
			// we acquired the mutex, so it has already sent the notification to the 
			// void.

			while (m_count.compare_exchange_strong(actual, now, rlx, rlx))
			{
				m_cv.wait(lock);
				actual = now; // don't let "actual" refresh
			}
		}
	private:
		using Lock = std::unique_lock<std::mutex>;
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::atomic<int> m_count;
	};
	
	class NopSemaphore
	{
	public:
		explicit NopSemaphore (const uint32_t _count = 0)	{}
	    void notify(uint32_t _count = 1)					{}
		void wait(uint32_t _count = 1)						{}
	};
};
