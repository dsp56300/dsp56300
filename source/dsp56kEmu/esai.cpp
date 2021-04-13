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
		if(!(m_tcr & M_TEM))
			return;

		++m_cyclesSinceWrite;
		if(m_cyclesSinceWrite <= g_cyclesPerSample)
			return;

		// Time to xfer samples!
		m_cyclesSinceWrite = 0;
		for (int i=0;i<6;i++) if (outputEnabled(i)) writeTXimpl(i,m_tx[i]);
		for (int i=0;i<4;i++) if (inputEnabled(i)) m_rx[i]=readRXimpl(i);
		m_frameCounter ^= 1;

		if (m_sr.test(M_TUE) && m_tcr.test(M_TEIE)) m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data_with_Exception_Status);
		else if (m_tcr.test(M_TIE))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data);
		if (!m_frameCounter && m_tcr.test(M_TLIE))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Last_Slot);

		m_sr.set(M_TUE, M_TDE);
		m_writtenTX = 0;
		m_hasReadStatus = 0;
	}

	void Esai::writeTX(size_t _index, TWord _val)
	{
		if(!outputEnabled(_index))
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
		if(!inputEnabled(_index))
			return 0;

		return m_rx[_index];
	}
}
