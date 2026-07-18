#pragma once

#include <vector>
#include <deque>
#include <mutex>

#include "hdi08.h"
#include "types.h"

namespace dsp56k
{
	class HDI08;

	class HDI08Queue
	{
	public:
		HDI08Queue();

		void writeRX(const std::vector<TWord>& _data);
		void writeRX(const TWord* _data, size_t _count);

		void writeHostFlags(uint8_t _flag0, uint8_t _flag1);

		void exec();

		void addHDI08(HDI08& _hdi08);

		bool rxEmpty() const;

		size_t size() const { return m_hdi08.size(); }
		HDI08* get(const size_t _index) const { return m_hdi08[_index]; }

	private:
		bool rxFull() const;
		void sendPendingData();

		static constexpr uint8_t HostFlagInvalid = 0xff;

		// m_dataRX entry encoding: a data word occupies bits 23..0 with bit 31 clear; a host-flag update
		// carries no data, sets bit 31, and stores HF0 in bit 24 / HF1 in bit 25. Flag changes are queued
		// as their own ordered entries (instead of being stamped onto the next data word) so a transition
		// with no data behind it - e.g. an HF1 reboot pulse - is still delivered, and a 1->0 change is
		// represented exactly instead of being OR-ed into a word that could keep the stale bit set.
		static constexpr TWord DataMask   = 0x00ffffff;
		static constexpr TWord FlagUpdate = 0x80000000;
		static constexpr TWord FlagHf0    = 0x01000000;
		static constexpr TWord FlagHf1    = 0x02000000;

		std::vector<HDI08*> m_hdi08;
		std::deque<TWord> m_dataRX;

		uint8_t m_lastHostFlag0 = HostFlagInvalid;
		uint8_t m_lastHostFlag1 = HostFlagInvalid;

		std::mutex m_mutex;
	};
}
