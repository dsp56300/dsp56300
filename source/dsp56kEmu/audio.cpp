#include "audio.h"

namespace dsp56k
{
	void Audio::terminate()
	{
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
		m_audioInputs.waitNotEmpty();
		_values = m_audioInputs.pop_front();
	}

	void Audio::writeTXimpl(const TxFrame& _values)
	{
		m_audioOutputs.waitNotFull();
		m_audioOutputs.push_back(_values);

		if (m_audioOutputs.size() >= (m_callbackSamples << 1))
			m_callback(this);
	}
}
