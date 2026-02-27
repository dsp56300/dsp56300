#pragma once

#include "types.h"

namespace dsp56k
{
	class DSP;

	class DspBoot
	{
	public:
		enum class State
		{
			Length,
			Address,
			Data,
			Finished
		};

		explicit DspBoot(DSP& _dsp);

		bool hdiWriteTX(const TWord& _val);

		bool finished() const { return m_state == State::Finished; }

		auto getLength() const { return m_length; }
		auto getInitialPC() const { return m_initialPC; }

	private:
		DSP& m_dsp;

		State m_state = State::Length;

		TWord m_length = 0;
		TWord m_initialPC = 0;

		TWord m_remaining = 0;
		TWord m_address = 0;
	};
}
