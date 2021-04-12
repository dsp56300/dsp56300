#include "esai.h"

#include "dsp.h"
#include "peripherals.h"

namespace dsp56k
{
	constexpr size_t g_dspFrequencyMHz = 100;
	constexpr size_t g_samplerate = 48000;

	constexpr size_t g_cyclesPerSample = g_dspFrequencyMHz * 1000 * 1000 / g_samplerate;
	
	Esai::Esai(IPeripherals& _periph) : m_periph(_periph)
	{
	}

	void Esai::exec()
	{
		++m_cyclesSinceWrite;

		// fire TX exception status if the DSP doesn't catch up
		if(m_cyclesSinceWrite > g_cyclesPerSample)
		{
			m_cyclesSinceWrite = 0;

			// Transmit interrupt exception
			if(bittest<TWord, M_TIE>(m_tcr))
				m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data_with_Exception_Status);

			bitset<TWord, M_TUE>(m_sr, 1);
		}
	}

	void Esai::writeTX(size_t _index, TWord _val)
	{
		m_cyclesSinceWrite = 0;

		bitset<TWord, M_TUE>(m_sr, 0);

		writeTXimpl(_index, _val);

		if(_index == 2)
		{
			if(bittest<TWord,M_TIE>(m_tcr))
				m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data);
			if(bittest<TWord,M_TLIE>(m_tcr))
				m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Last_Slot);
		}
	}

	TWord Esai::readRX(size_t _index)
	{
		if(!bittest(m_rcr, M_RE0 + _index))
			return 0;

		const auto res = readRXimpl(_index);

		bitset<TWord, M_RFS>(m_sr, m_frameSyncDSPRead);

		return res;
	}
}
