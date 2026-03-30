#include "audio.h"

namespace dsp56k
{
	Audio::Audio(const bool _useRingBuffers/* = true*/)
		: m_callback([](Audio*) {})
		, m_useRingBuffers(_useRingBuffers)
	{
		if(_useRingBuffers)
		{
			m_readRxCallback = [this](RxFrame& _values)
			{
				m_audioInputs.waitNotEmpty();
				_values = m_audioInputs.pop_front();
			};

			m_writeTxCallback = [this](const TxFrame& _values)
			{
				m_audioOutputs.waitNotFull();
				m_audioOutputs.push_back(_values);
				m_callback(this);
			};
		}
	}

	void Audio::terminate()
	{
		setCallback([](Audio*) {});

		while(true)
		{
			if(!m_audioOutputs.empty())
				m_audioOutputs.pop_front();
			else if(!m_audioInputs.full())
				m_audioInputs.push_back({});
			else
				break;
		}
	}

	void Audio::readRXimpl(RxFrame& _values)
	{
		m_readRxCallback(_values);
	}

	void Audio::writeTXimpl(const TxFrame& _values)
	{
		m_writeTxCallback(_values);
	}
}
