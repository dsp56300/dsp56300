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
		if(m_tcr & M_TEM)
		{
			++m_cyclesSinceWrite;
			if(m_cyclesSinceWrite > g_cyclesPerSample)	// Time to xfer samples!
			{
				m_cyclesSinceWrite = 0;
				// Are we about to underrun?
				bool underrun=false;
				if (bittest<TWord,M_TUE>(m_sr) && bittest<TWord,M_TDE>(m_sr) && bittest<TWord,M_TEIE>(m_tcr))
				{
					m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data_with_Exception_Status);
					underrun = true;
				}

				for (int i=0;i<6;i++) if (m_tcr & (1<<i)) writeTXimpl(i,m_tx[i]);
				for (int i=0;i<6;i++) if (m_rcr & (1<<i)) m_rx[i]=readRXimpl(i);
				bitset<TWord, M_TUE>(m_sr, 1);
				bitset<TWord, M_TDE>(m_sr, 1);
				m_frameCounter ^= 1;
				m_writtenTX = 0;
				m_hasReadStatus = 0;
				if (!underrun)
				{
					if (!m_frameCounter && bittest<TWord,M_TLIE>(m_tcr))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Last_Slot);
					else if (bittest<TWord,M_TIE>(m_tcr))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data);
				}
			}
		}
	}

	void Esai::writeTX(size_t _index, TWord _val)
	{
		if(!bittest(m_tcr, M_TE0 + _index))
			return;
		m_tx[_index] = _val;
		m_writtenTX |= (1<<_index);
		if(m_writtenTX == (m_tcr & M_TEM))
		{
			if (m_hasReadStatus) bitset<TWord, M_TUE>(m_sr, 0);
			bitset<TWord, M_TDE>(m_sr, 0);
		}
	}

	TWord Esai::readRX(size_t _index)
	{
		if(!bittest(m_rcr, M_RE0 + _index))
			return 0;

		return m_rx[_index];
	}
}
