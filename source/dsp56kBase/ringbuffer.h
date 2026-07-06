#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>
#include <thread>

#include "dspassert.h"

#include "semaphore.h"

namespace dsp56k
{
	template<typename T, size_t C, bool Lock, bool StackAlloc = true> class RingBuffer
	{
	public:
		RingBuffer() : m_writeCount(0), m_readCount(0), m_readSem(0), m_writeSem(static_cast<int>(C))
		{
			static_assert(C>0, "C needs to be greater than 1");
			static_assert((C&(C-1)) == 0, "C needs to be power of two");
			initBuffer(m_data);
		}

		// std::atomic disables the implicit move/copy members. Callers (e.g. BypassBuffer's
		// std::vector<RingBuffer<...>>) only ever move an instance while it is exclusively owned
		// by the moving thread (container growth), never concurrently with another thread's
		// push/pop, so relaxed loads of the counters are sufficient here.
		RingBuffer(RingBuffer&& _other) noexcept
			: m_data(std::move(_other.m_data))
			, m_writeCount(_other.m_writeCount.load(std::memory_order_relaxed))
			, m_readCount(_other.m_readCount.load(std::memory_order_relaxed))
			, m_readSem(std::move(_other.m_readSem))
			, m_writeSem(std::move(_other.m_writeSem))
		{
		}

		RingBuffer& operator=(RingBuffer&& _other) noexcept
		{
			if (this == &_other)
				return *this;

			m_data = std::move(_other.m_data);
			m_writeCount.store(_other.m_writeCount.load(std::memory_order_relaxed), std::memory_order_relaxed);
			m_readCount.store(_other.m_readCount.load(std::memory_order_relaxed), std::memory_order_relaxed);
			m_readSem = std::move(_other.m_readSem);
			m_writeSem = std::move(_other.m_writeSem);

			return *this;
		}

		constexpr static size_t capacity()	{ return C; }
		bool empty() const					{ return m_readCount.load(std::memory_order_acquire) == m_writeCount.load(std::memory_order_acquire); }
		bool full() const					{ return size() >= C; }
		size_t size() const					{ return m_writeCount.load(std::memory_order_acquire) - m_readCount.load(std::memory_order_acquire); }
		size_t remaining() const			{ return (C - size()); }

		void push_back( const T& _val )
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			const auto writeIdx = m_writeCount.load(std::memory_order_relaxed);
			m_data[wrapCounter(writeIdx)] = _val;

			// counter needs to be published (release) AFTER data has been written, so that a reader thread
			// observing the new count via an acquire load is guaranteed to also see the data
			m_writeCount.store(writeIdx + 1, std::memory_order_release);

			m_readSem.notify();
		}

		void push_back( T&& _val )
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			const auto writeIdx = m_writeCount.load(std::memory_order_relaxed);
			m_data[wrapCounter(writeIdx)] = std::move(_val);

			m_writeCount.store(writeIdx + 1, std::memory_order_release);

			m_readSem.notify();
		}

		template<typename TFunc>
		void emplace_back(const TFunc& _fillEntry)
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			const auto writeIdx = m_writeCount.load(std::memory_order_relaxed);
			_fillEntry(m_data[wrapCounter(writeIdx)]);

			m_writeCount.store(writeIdx + 1, std::memory_order_release);

			m_readSem.notify();
		}

		template<typename TFunc>
		void emplace_back(size_t _count, const TFunc& _fillEntry)
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait(static_cast<uint32_t>(_count));

			const auto writeIdx = m_writeCount.load(std::memory_order_relaxed);
			for (size_t i=0; i<_count; ++i)
				_fillEntry(i, m_data[wrapCounter(writeIdx)]);

			m_writeCount.store(writeIdx + _count, std::memory_order_release);

			m_readSem.notify(static_cast<uint32_t>(_count));
		}

		template<typename TFunc>
		void pop_front(const TFunc& _readCallback)
		{
			m_readSem.wait();

			const auto readIdx = m_readCount.load(std::memory_order_relaxed);
			_readCallback(m_data[wrapCounter(readIdx)]);
	//		assert( !empty() && "ring buffer is already empty!" );

			m_readCount.store(readIdx + 1, std::memory_order_release);

			m_writeSem.notify();
		}

		template<typename TFunc>
		void pop_front(const size_t _count, const TFunc& _readCallback)
		{
			m_readSem.wait(static_cast<uint32_t>(_count));

			for (size_t i=0; i<_count; ++i)
			{
				const auto readIdx = m_readCount.load(std::memory_order_relaxed);
				_readCallback(i, m_data[wrapCounter(readIdx)]);
				m_readCount.store(readIdx + 1, std::memory_order_release);
			}

			m_writeSem.notify(static_cast<uint32_t>(_count));
		}

		T pop_front()
		{
			m_readSem.wait();

			const auto readIdx = m_readCount.load(std::memory_order_relaxed);
			T res = std::move(m_data[wrapCounter(readIdx)]);
	//		assert( !empty() && "ring buffer is already empty!" );

			m_readCount.store(readIdx + 1, std::memory_order_release);

			m_writeSem.notify();

			return res;
		}

		T& operator[](const size_t _i)
		{
			return get(_i);
		}

		const T& operator[](const size_t _i) const
		{
			return const_cast<RingBuffer*>(this)->get(_i);
		}

		const T& front() const
		{
			return m_data[wrapCounter(m_readCount.load(std::memory_order_acquire))];
		}

		T& front()
		{
			return m_data[wrapCounter(m_readCount.load(std::memory_order_acquire))];
		}

		void clear()
		{
			while( !empty() )
				pop_front();
		}

		void waitNotEmpty() const
		{
			if constexpr(Lock)
				return;
			while(empty())
				std::this_thread::yield();
		}

		void waitNotFull() const
		{
			if constexpr(Lock)
				return;
			while(full())
				std::this_thread::yield();
		}

	private:
		static size_t wrapCounter( const size_t& _counter )
		{
			return _counter & (C-1);
		}

		T& get( size_t _i )
		{
			convertIdx( _i );

			return m_data[_i];
		}

		void convertIdx( size_t& _i ) const
		{
			_i += m_readCount;

			_i &= C-1;
		}

		static void initBuffer(std::array<T, C>& _buffer)
		{
		}

		static void initBuffer(std::vector<T>& _buffer)
		{
			_buffer.resize(C);
		}

		using MemoryBuffer = std::conditional_t<StackAlloc, std::array<T, C>, std::vector<T>>;

		MemoryBuffer		m_data;

		std::atomic<size_t>	m_writeCount;
		std::atomic<size_t>	m_readCount;

		typedef std::conditional_t<Lock, SpscSemaphoreWithCount, NopSemaphore> Sem;

		Sem					m_readSem;
		Sem					m_writeSem;

	public:
		static void test()
		{
			RingBuffer<int,10, false> rb;

			assert( rb.size() == 0 );
			assert( rb.empty() );
			assert( rb.remaining() == 10 );

			rb.push_back( 3 );
			rb.push_back( 4 );
			rb.push_back( 5 );
			rb.push_back( 6 );

			assert( rb.size() == 4 );
			assert( !rb.empty() );
			assert( rb.remaining() == 6 );

			assert( rb[2] == 5 );

			rb.pop_front();

			assert( rb.size() == 3 );
			assert( !rb.empty() );
			assert( rb.remaining() == 7 );

			assert( rb[2] == 6 );
			assert( rb[0] == 4 );
			assert( rb.front() == 4 );

			rb.pop_front();
			rb.pop_front();
			rb.pop_front();

			assert( rb.size() == 0 );
			assert( rb.empty() );
			assert( rb.remaining() == 10 );

			rb.push_back(77);

			assert( rb.size() == 1 );
			assert( !rb.empty() );
			assert( rb.remaining() == 9 );

			assert( rb.front() == 77 );
			assert( rb[0] == 77 );
		}
	};
}
