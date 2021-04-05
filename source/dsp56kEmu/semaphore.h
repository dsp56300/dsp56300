#pragma once

#include <mutex>

namespace ceLib
{
	class Semaphore
	{
	public:
		explicit Semaphore (const int _count = 0) : m_count(_count) 
	    {
	    }
	    
	    void notify()
		{
	        Lock lock(m_mutex);
	        m_count++;
	        //notify the waiting thread
	        m_cv.notify_one();
	    }

	    void wait()
		{
	        Lock lock(m_mutex);

	    	while(m_count == 0)
			{
	            // wait on the mutex until notify is called
	            m_cv.wait(lock);
	        }

	    	m_count--;
	    }
	private:
		using Lock = std::unique_lock<std::mutex>;
	    std::mutex m_mutex;
	    std::condition_variable m_cv;
	    int m_count;
	};

	class NopSemaphore
	{
	public:
		explicit NopSemaphore (const int _count = 0) {}
	    void notify()	    {}
		void wait()			{}
	};
};
