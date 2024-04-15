#pragma once

#include <algorithm>
#include <array>
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

		size_t capacity() const		{ return C; }
		bool empty() const			{ return m_readCount == m_writeCount; }
		bool full() const			{ return size() >= C; }
		size_t size() const			{ return m_writeCount - m_readCount; }
		size_t remaining() const	{ return (C - size()); }

		void push_back( const T& _val )
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			m_data[wrapCounter(m_writeCount)] = _val;

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			++m_writeCount;

			m_readSem.notify();
		}
		
		void push_back( T&& _val )
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			m_data[wrapCounter(m_writeCount)] = std::move(_val);

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			++m_writeCount;

			m_readSem.notify();
		}

		template<typename TFunc>
		void emplace_back(const TFunc& _fillEntry)
		{
	//		assert( m_usage < C && "ring buffer is already full!" );

			m_writeSem.wait();

			_fillEntry(m_data[wrapCounter(m_writeCount)]);

			// usage need to be incremented AFTER data has been written, otherwise, reader thread would read incomplete data
			++m_writeCount;

			m_readSem.notify();
		}

		template<typename TFunc>
		void pop_front(const TFunc& _readCallback)
		{
			m_readSem.wait();

			_readCallback(front());
	//		assert( !empty() && "ring buffer is already empty!" );

			++m_readCount;

			m_writeSem.notify();
		}

		T pop_front()
		{
			m_readSem.wait();

			T res = std::move(front());
	//		assert( !empty() && "ring buffer is already empty!" );

			++m_readCount;

			m_writeSem.notify();

			return res;
		}

		void removeAt( size_t i )
		{
			if( !i )
			{
				pop_front();
				return;
			}

			convertIdx(i);

			std::swap( m_data[i], m_data[wrapCounter(m_readCount)] );

			pop_front();
		}

		T& operator[](size_t i)
		{
			return get(i);
		}

		const T& operator[](size_t i) const
		{
			return const_cast< RingBuffer<T,C, Lock>* >(this)->get(i);
		}

		const T& front() const
		{
			return m_data[wrapCounter(m_readCount)];
		}
		
		T& front()
		{
			return m_data[wrapCounter(m_readCount)];
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

		T& get( size_t i )
		{
			convertIdx( i );

			return m_data[i];
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

		size_t				m_writeCount;
		size_t				m_readCount;

		typedef std::conditional_t<Lock, SpscSemaphore, NopSemaphore> Sem;

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
