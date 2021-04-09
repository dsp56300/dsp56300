#include "audio.h"

namespace dsp56k
{
	TWord Audio::readRXimpl(size_t _index)
	{
		m_frameSyncDSPStatus = m_frameSyncDSPRead;

		incFrameSync(m_frameSyncDSPRead);

		m_audioInputs[_index].waitNotEmpty();
		const auto res = m_audioInputs[_index].pop_front();
		return res;
	}

	void Audio::writeTXimpl(size_t _index, TWord _val)
	{
		incFrameSync(m_frameSyncDSPWrite);
		m_audioOutputs[_index].waitNotFull();
		m_audioOutputs[_index].push_back(_val);
	}
}
