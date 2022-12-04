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
		bool needsToWaitforHostFlags(uint8_t _flag0, uint8_t _flag1) const;
		void sendPendingData();

		static constexpr uint8_t HostFlagInvalid = 0xff;

		struct Data
		{
			std::vector<TWord> data;
			uint8_t hostFlag0 = 0;
			uint8_t hostFlag1 = 0;
		};

		std::vector<HDI08*> m_hdi08;
		std::deque<TWord> m_dataRX;

		uint8_t m_lastHostFlag0 = HostFlagInvalid;
		uint8_t m_lastHostFlag1 = HostFlagInvalid;

		uint32_t m_nextHostFlags = 0;

		std::mutex m_mutex;
	};
}
