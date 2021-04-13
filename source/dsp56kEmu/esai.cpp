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
				if (m_sr.test(M_TUE, M_TDE) && m_tcr.test(M_TEIE))
				{
					m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data_with_Exception_Status);
					underrun = true;
				}

				for (int i=0;i<6;i++) if (m_tcr.test(static_cast<TcrBits>(i))) writeTXimpl(i,m_tx[i]);
				for (int i=0;i<6;i++) if (m_rcr.test(static_cast<RcrBits>(i))) m_rx[i]=readRXimpl(i);
				m_sr.set(M_TUE, M_TDE);
				m_frameCounter ^= 1;
				m_writtenTX = 0;
				m_hasReadStatus = 0;
				if (!underrun)
				{
					if (!m_frameCounter && m_tcr.test(M_TLIE))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Last_Slot);
					else if (m_tcr.test(M_TIE))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data);
				}
			}
		}
	}

	void Esai::writeTX(size_t _index, TWord _val)
	{
		if(!m_tcr.test(static_cast<TcrBits>(M_TE0 + _index)))
			return;
		m_tx[_index] = _val;
		m_writtenTX |= (1<<_index);
		if(m_writtenTX == (m_tcr & M_TEM))
		{
			if (m_hasReadStatus) m_sr.clear(M_TUE);
			m_sr.clear(M_TDE);
		}
	}

	TWord Esai::readRX(size_t _index)
	{
		if(!m_rcr.test(static_cast<RcrBits>(M_RE0 + _index)))
			return 0;

		return m_rx[_index];
	}
}
