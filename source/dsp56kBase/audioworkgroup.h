#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace dsp56k
{
	// The audio workgroup of the host, macOS 11 and later. A no-op elsewhere.
	//
	// Threads that produce the audio the host waits for belong into it. Apple Silicon schedules threads that are not
	// members by the load of the rest of their process: while that looks idle, they stay on the efficiency cores and
	// the host waits for audio that is late. Members count as part of the host's audio deadline instead.
	class AudioWorkgroup
	{
	public:
		// Any thread. _workgroup is an os_workgroup_t or nullptr, it is retained while it is set. _samplerate and
		// _blocksize describe the host's audio period, which joined realtime threads use as their time constraint
		static void set(void* _workgroup, int _samplerate, int _blocksize);

		// A thread that produces audio creates a Member on its stack and calls update() regularly. update() joins the
		// workgroup that is set or leaves the one that is gone. The destructor leaves, a thread must not end while it
		// is a member. Only realtime threads join, a Member of any other thread does nothing
		class Member
		{
		public:
			explicit Member(bool _realtime = true);
			~Member();

			Member(const Member&) = delete;
			Member& operator=(const Member&) = delete;

			void update()
			{
				const auto generation = s_generation.load(std::memory_order_relaxed);
				if(generation != m_generation)
					rejoin(generation);
			}

		private:
			void rejoin(uint32_t _generation);
			void leave();

			const bool m_realtime;
			uint32_t m_generation = 0;
			void* m_joined = nullptr;	// os_workgroup_t, retained while this thread is a member
			int m_samplerate = 0;
			int m_blocksize = 0;
			alignas(8) std::array<uint8_t, 64> m_token{};	// os_workgroup_join_token_s
		};

	private:
		static inline std::atomic<uint32_t> s_generation{0};
	};
}
