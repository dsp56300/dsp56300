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

		for(size_t i=0; i<_count; ++i)
			m_dataRX.push_back(_data[i] & DataMask);

		sendPendingData();
	}

	void HDI08Queue::writeHostFlags(uint8_t _flag0, uint8_t _flag1)
	{
		std::lock_guard lock(m_mutex);

		if(m_lastHostFlag0 == _flag0 && m_lastHostFlag1 == _flag1)
			return;

		m_lastHostFlag0 = _flag0;
		m_lastHostFlag1 = _flag1;

		TWord entry = FlagUpdate;
		if(_flag0)
			entry |= FlagHf0;
		if(_flag1)
			entry |= FlagHf1;

		m_dataRX.push_back(entry);

		sendPendingData();
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

	void HDI08Queue::sendPendingData()
	{
		while(!m_dataRX.empty())
		{
			auto d = m_dataRX.front();

			if(d & FlagUpdate)
			{
				// Deliver the flag change through the pending-host-flags latch (applied when the DSP next
				// reads its status register) and refuse to move on to the next flag until the DSP has
				// consumed this one. This preserves flag PULSES: firmware announces an address by pulsing
				// HF0 1->0, and the DSP has to catch the HF0=1 edge on one of its own reads (it raises HF3
				// in response). Writing flags straight to the register, or overwriting a pending value the
				// DSP has not read yet, collapses the pulse and the DSP never enters its address-receive
				// state. Non-blocking: if the previous flag is still pending we leave this entry in place
				// and retry on the next exec().
				// Defer the flag change while (a) the previous flag is still pending (the DSP has not observed
				// it yet - preserves the pulse) or (b) the DSP still has unread words in its RX from the
				// previous phase (changing the flag now would re-tag those words - this is the old "wait for
				// RX empty" guard). Retry on the next exec() (a DSP read pumps us via the callbacks).
				bool defer = false;
				for (auto* hdi08 : m_hdi08)
				{
					if(hdi08->hasPendingHostFlags01() || hdi08->hasRXData())
					{
						defer = true;
						break;
					}
				}
				if(defer)
					break;

				const uint32_t hf01 =
					(((d & FlagHf0) ? 1u : 0u) << HDI08::HSR_HF0) |
					(((d & FlagHf1) ? 1u : 0u) << HDI08::HSR_HF1);

				for (auto* hdi08 : m_hdi08)
					hdi08->setPendingHostFlags01(hf01);
			}
			else
			{
				if(rxFull())
					break;

				for (auto* hdi08 : m_hdi08)
					hdi08->writeRX(&d, 1);
			}

			m_dataRX.pop_front();
		}
	}
}
