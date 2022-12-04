#include "audio.h"

namespace dsp56k
{
	void Audio::readRXimpl(std::array<TWord, 4>& _values)
	{
		m_frameSyncDSPStatus = m_frameSyncDSPRead;

		incFrameSync(m_frameSyncDSPRead);

		m_audioInputs.waitNotEmpty();
		_values = m_audioInputs.pop_front();
	}

	void Audio::writeTXimpl(const std::array<TWord, 6>& _values)
	{
		incFrameSync(m_frameSyncDSPWrite);

		m_audioOutputs.waitNotFull();
		m_audioOutputs.push_back(_values);

		if (m_callback && m_audioOutputs.size() >= (m_callbackSamples << 1))
			m_callback(this);
	}
}
