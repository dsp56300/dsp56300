#include "esai.h"

#include "dsp.h"
#include "peripherals.h"

namespace dsp56k
{
	Esai::Esai(IPeripherals& _periph) : m_periph(_periph)
	{
		m_tx.fill(0);
		m_rx.fill(0);
	}

	void Esai::exec()
	{
		if(!(m_tcr & M_TEM))
			return;

		const auto clock = m_periph.getDSP().getInstructionCounter();
		const auto diff = delta(clock, m_lastClock);
		m_lastClock = clock;

		m_cyclesSinceWrite+=diff;
		if(m_cyclesSinceWrite <= m_cyclesPerSample)
			return;

		// Time to xfer samples!
		m_cyclesSinceWrite -= m_cyclesPerSample;
		for (int i=0;i<6;i++) if (outputEnabled(i)) writeTXimpl(i,m_tx[i]);
		for (int i=0;i<4;i++) if (inputEnabled(i)) m_rx[i]=readRXimpl(i);
		if (m_sr.test(M_TFS)) m_sr.clear(M_TFS); else m_sr.set(M_TFS);

//		if (m_sr.test(M_TUE) && m_tcr.test(M_TEIE)) m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data_with_Exception_Status);
//		else
		if (m_tcr.test(M_TIE))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Data);
		if (m_sr.test(M_TFS) && m_tcr.test(M_TLIE))  m_periph.getDSP().injectInterrupt(Vba_ESAI_Transmit_Last_Slot);

		m_sr.set(M_TUE, M_TDE);
		m_writtenTX = 0;
		m_hasReadStatus = 0;
	}

	void Esai::writeTX(uint32_t _index, TWord _val)
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

	TWord Esai::readRX(uint32_t _index)
	{
		if(!inputEnabled(_index))
			return 0;

		return m_rx[_index];
	}
}
