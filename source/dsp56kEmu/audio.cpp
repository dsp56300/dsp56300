#include "audio.h"

namespace dsp56k
{
	Audio::Audio(const bool _useRingBuffers/* = true*/)
		: m_callback([](Audio*) {})
		, m_useRingBuffers(_useRingBuffers)
	{
		if(_useRingBuffers)
		{
			m_readRxCallback = [this](uint64_t& _frameIndex, RxFrame& _values)
			{
				m_audioInputs.waitNotEmpty();
				_values = m_audioInputs.pop_front();
				++_frameIndex;
			};

			m_writeTxCallback = [this](uint64_t& _frameIndex, const TxFrame& _values)
			{
				m_audioOutputs.waitNotFull();
				m_audioOutputs.push_back(_values);
				++_frameIndex;
				m_callback(this);
			};
		}
	}

	void Audio::terminate()
	{
		m_terminated.store(true, std::memory_order_release);

		setCallback([](Audio*) {});

		if (m_useRingBuffers)
		{
			while(true)
			{
				if(m_audioOutputs.size() > (m_audioOutputs.capacity()>>1))
					m_audioOutputs.pop_front();
				else if(m_audioInputs.size() < (m_audioInputs.capacity()>>1))
					m_audioInputs.push_back({});
				else
					break;
			}
		}
	}

	void Audio::readRXimpl(RxFrame& _values)
	{
		if (m_terminated.load(std::memory_order_acquire))
			return;
		m_readRxCallback(m_readFrameIndex, _values);
	}

	void Audio::writeTXimpl(const TxFrame& _values)
	{
		if (m_terminated.load(std::memory_order_acquire))
			return;
		m_writeTxCallback(m_writeFrameIndex, _values);
	}
}
