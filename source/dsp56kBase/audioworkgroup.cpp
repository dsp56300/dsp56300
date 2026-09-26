#include "audioworkgroup.h"

#include "logging.h"
#include "threadtools.h"

#include <mutex>

#ifdef __APPLE__
#	include <os/workgroup.h>
#endif

namespace dsp56k
{
	namespace
	{
		std::mutex g_mutex;
		void* g_workgroup = nullptr;	// os_workgroup_t, retained while it is set
		int g_samplerate = 0;
		int g_blocksize = 0;
	}

	void AudioWorkgroup::set(void* _workgroup, const int _samplerate, const int _blocksize)
	{
#ifdef __APPLE__
		if(__builtin_available(macOS 11.0, *))
		{
			std::lock_guard lock(g_mutex);

			if(_workgroup == g_workgroup && _samplerate == g_samplerate && _blocksize == g_blocksize)
				return;

			if(_workgroup)
				os_retain(_workgroup);
			if(g_workgroup)
				os_release(g_workgroup);

			g_workgroup = _workgroup;
			g_samplerate = _samplerate;
			g_blocksize = _blocksize;

			s_generation.fetch_add(1, std::memory_order_relaxed);
		}
#else
		(void)_workgroup;
		(void)_samplerate;
		(void)_blocksize;
#endif
	}

	AudioWorkgroup::Member::Member(const bool _realtime) : m_realtime(_realtime)
	{
	}

	AudioWorkgroup::Member::~Member()
	{
		leave();
	}

	void AudioWorkgroup::Member::rejoin(const uint32_t _generation)
	{
		m_generation = _generation;

#ifdef __APPLE__
		if(!m_realtime)
			return;

		if(__builtin_available(macOS 11.0, *))
		{
			void* workgroup;
			int samplerate;
			int blocksize;
			{
				std::lock_guard lock(g_mutex);
				workgroup = g_workgroup;
				samplerate = g_samplerate;
				blocksize = g_blocksize;
				if(workgroup)
					os_retain(workgroup);
			}

			if(workgroup && workgroup == m_joined)
			{
				// still a member, only the audio period changed
				os_release(workgroup);
				if(samplerate != m_samplerate || blocksize != m_blocksize)
				{
					m_samplerate = samplerate;
					m_blocksize = blocksize;
					ThreadTools::setCurrentThreadRealtimeParameters(samplerate, blocksize);
				}
				return;
			}

			leave();

			if(!workgroup)
				return;

			static_assert(sizeof(os_workgroup_join_token_s) <= sizeof(m_token), "join token storage too small");
			auto* token = reinterpret_cast<os_workgroup_join_token_s*>(m_token.data());

			if(const int result = os_workgroup_join(static_cast<os_workgroup_t>(workgroup), token))
			{
				os_release(workgroup);
				LOG("Failed to join the audio workgroup of the host, error " << result);
				return;
			}

			m_joined = workgroup;
			m_samplerate = samplerate;
			m_blocksize = blocksize;
			ThreadTools::setCurrentThreadRealtimeParameters(samplerate, blocksize);
			LOG("Joined the audio workgroup of the host, " << blocksize << " frames at " << samplerate << " Hz");
		}
#endif
	}

	void AudioWorkgroup::Member::leave()
	{
#ifdef __APPLE__
		if(!m_joined)
			return;

		if(__builtin_available(macOS 11.0, *))
		{
			os_workgroup_leave(static_cast<os_workgroup_t>(m_joined), reinterpret_cast<os_workgroup_join_token_s*>(m_token.data()));
			os_release(m_joined);
		}

		m_joined = nullptr;
		m_samplerate = m_blocksize = 0;

		// without the host's audio period, fall back to the parameters of a thread that does not know it
		ThreadTools::setCurrentThreadRealtimeParameters(0, 0);
		LOG("Left the audio workgroup of the host");
#endif
	}
}
