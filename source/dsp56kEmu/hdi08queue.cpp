#include "hdi08queue.h"

namespace dsp56k
{
	HDI08Queue::HDI08Queue() = default;

	void HDI08Queue::writeRX(const std::vector<TWord>& _data)
	{
		if(_data.empty())
			return;

		writeRX(&_data.front(), _data.size());
	}

	void HDI08Queue::writeRX(const TWord* _data, size_t _count)
	{
		if(_count == 0 || !_data)
			return;

		std::lock_guard lock(m_mutex);

		m_dataRX.push_back(_data[0] | m_nextHostFlags);
		m_nextHostFlags = 0;

		for(size_t i=1; i<_count; ++i)
			m_dataRX.push_back(_data[i]);

		sendPendingData();
	}

	void HDI08Queue::writeHostFlags(uint8_t _flag0, uint8_t _flag1)
	{
		std::lock_guard lock(m_mutex);

		if(m_lastHostFlag0 == _flag0 && m_lastHostFlag1 == _flag1)
			return;

		m_lastHostFlag0 = _flag0;
		m_lastHostFlag1 = _flag1;

		m_nextHostFlags |= static_cast<TWord>(_flag0) << 24;
		m_nextHostFlags |= static_cast<TWord>(_flag1) << 25;
		m_nextHostFlags |= 0x80000000;
	}

	void HDI08Queue::exec()
	{
		std::lock_guard lock(m_mutex);

		sendPendingData();
	}

	void HDI08Queue::addHDI08(HDI08& _hdi08)
	{
		std::lock_guard lock(m_mutex);
		m_hdi08.push_back(&_hdi08);
	}

	bool HDI08Queue::rxEmpty() const
	{
		if(!m_dataRX.empty())
			return false;

		for (const auto* hdi08 : m_hdi08)
		{
			if(hdi08->hasRXData())
				return false;
		}
		return true;
	}

	inline bool HDI08Queue::rxFull() const
	{
		for (const auto* hdi08 : m_hdi08)
		{
			if(hdi08->dataRXFull())
				return true;
		}
		return false;
	}

	bool HDI08Queue::needsToWaitforHostFlags(uint8_t _flag0, uint8_t _flag1) const
	{
		for (const auto* hdi08 : m_hdi08)
		{
			if(hdi08->needsToWaitForHostFlags(_flag0, _flag1))
				return true;
		}
		return false;
	}

	void HDI08Queue::sendPendingData()
	{
		while(!m_dataRX.empty() && !rxFull())
		{
			auto d = m_dataRX.front();

			if(d & 0x80000000)
			{
				const auto hostFlag0 = static_cast<uint8_t>((d >> 24) & 1);
				const auto hostFlag1 = static_cast<uint8_t>((d >> 25) & 1);

				if(needsToWaitforHostFlags(hostFlag0, hostFlag1))
					break;

				for (auto* hdi08 : m_hdi08)
					hdi08->setHostFlagsWithWait(hostFlag0, hostFlag1);

				d &= 0xffffff;
			}

			for (auto* hdi08 : m_hdi08)
				hdi08->writeRX(&d, 1);

			m_dataRX.pop_front();
		}
	}
}
