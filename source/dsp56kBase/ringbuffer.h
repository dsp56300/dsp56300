#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include <thread>
#include <atomic>

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

		// Movable so a Lock=false ring can live in a std::vector that reallocates (e.g. BypassBuffer). A ring
		// is only ever moved single-threaded (no concurrent access), so lifting the counters out of their
		// atomics with a relaxed load is safe. Only ever instantiated for Lock=false: the Lock=true semaphore
		// (SpscSemaphoreWithCount) is non-movable, so moving a Lock=true ring is a compile error, as intended.
		RingBuffer(RingBuffer&& _other) noexcept
			: m_data(std::move(_other.m_data))
			, m_writeCount(_other.m_writeCount.load(std::memory_order_relaxed))
			, m_readCount(_other.m_readCount.load(std::memory_order_relaxed))
			, m_readSem(std::move(_other.m_readSem))
			, m_writeSem(std::move(_other.m_writeSem))
		{
		}

		constexpr static size_t capacity()	{ return C; }
		bool empty() const					{ return loadReadAcq() == loadWriteAcq(); }
		bool full() const					{ return size() >= C; }
		size_t size() const					{ return loadWriteAcq() - loadReadAcq(); }
		size_t remaining() const			{ return (C - size()); }

		void push_back( const T& _val )
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			m_data[wrapCounter(loadWriteRlx())] = _val;

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			incWriteCount(1);

			m_readSem.notify();
		}

		void push_back( T&& _val )
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			m_data[wrapCounter(loadWriteRlx())] = std::move(_val);

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			incWriteCount(1);

			m_readSem.notify();
		}

		bool try_push_back(const T& _val)
		{
			if(!m_writeSem.tryWait())
				return false;

			m_data[wrapCounter(loadWriteRlx())] = _val;
			incWriteCount(1);
			m_readSem.notify();
			return true;
		}

		bool try_push_back(T&& _val)
		{
			if(!m_writeSem.tryWait())
				return false;

			m_data[wrapCounter(loadWriteRlx())] = std::move(_val);
			incWriteCount(1);
			m_readSem.notify();
			return true;
		}

		template<typename TFunc>
		void emplace_back(const TFunc& _fillEntry)
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			_fillEntry(m_data[wrapCounter(loadWriteRlx())]);

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			incWriteCount(1);

			m_readSem.notify();
		}

		template<typename TFunc>
		void emplace_back(size_t _count, const TFunc& _fillEntry)
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait(static_cast<uint32_t>(_count));

			for (size_t i=0; i<_count; ++i)
				_fillEntry(i, m_data[wrapCounter(loadWriteRlx())]);

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			incWriteCount(_count);

			m_readSem.notify(static_cast<uint32_t>(_count));
		}

		template<typename TFunc>
		void pop_front(const TFunc& _readCallback)
		{
			m_readSem.wait();

			_readCallback(front());
	//		assert( !empty() && "ring buffer is already empty!" );

			incReadCount(1);

			m_writeSem.notify();
		}

		template<typename TFunc>
		void pop_front(const size_t _count, const TFunc& _readCallback)
		{
			m_readSem.wait(static_cast<uint32_t>(_count));

			for (size_t i=0; i<_count; ++i)
			{
				_readCallback(i, front());
				incReadCount(1);
			}

			m_writeSem.notify(static_cast<uint32_t>(_count));
		}

		T pop_front()
		{
			m_readSem.wait();

			T res = std::move(front());
	//		assert( !empty() && "ring buffer is already empty!" );

			incReadCount(1);

			m_writeSem.notify();

			return res;
		}

		bool try_pop_front(T& _result)
		{
			if(!m_readSem.tryWait())
				return false;

			_result = std::move(front());
			incReadCount(1);
			m_writeSem.notify();
			return true;
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
			return m_data[wrapCounter(loadReadRlx())];
		}
		
		T& front()
		{
			return m_data[wrapCounter(loadReadRlx())];
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
		// Single-writer counter bumps: the producer owns m_writeCount, the consumer owns m_readCount, so a
		// relaxed load of our own value + a release store is race-free and cheaper than a locked fetch_add.
		// The release pairs with the acquire loads below so a cross-thread reader that observes the new count
		// also observes the data written before it.
		size_t loadWriteAcq() const			{ return m_writeCount.load(std::memory_order_acquire); }
		size_t loadReadAcq() const			{ return m_readCount.load(std::memory_order_acquire); }
		size_t loadWriteRlx() const			{ return m_writeCount.load(std::memory_order_relaxed); }
		size_t loadReadRlx() const			{ return m_readCount.load(std::memory_order_relaxed); }
		void incWriteCount(const size_t _n)	{ m_writeCount.store(m_writeCount.load(std::memory_order_relaxed) + _n, std::memory_order_release); }
		void incReadCount(const size_t _n)	{ m_readCount.store(m_readCount.load(std::memory_order_relaxed) + _n, std::memory_order_release); }

		static size_t wrapCounter( const size_t _counter )
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
			_i += loadReadAcq();

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

		// Atomic on ALL rings. The non-blocking empty()/size()/full() queries and the Lock=false spin loops
		// (waitNotEmpty/waitNotFull) read these cross-thread, so plain counters race on weakly-ordered ARM -
		// the compiler hoists the load out of the spin and it hangs (this is why the lock-free ring "did not
		// work on aarch64"; it now does). Movability for std::vector<> use is preserved by the move ctor.
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
