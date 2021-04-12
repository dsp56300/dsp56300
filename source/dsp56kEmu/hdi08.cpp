#include "dsp.h"
#include "interrupts.h"
#include "hdi08.h"

namespace dsp56k
{
	void HDI08::exec()
	{
		if (m_hcr&1 && !m_data.empty())
		{
			m_hcr=(((m_data[0]>>24) & 3) << 3) | (m_hcr&0xFFFE7);
			m_periph.getDSP().injectInterrupt(Vba_Host_Receive_Data_Full);
		}
	}

	void HDI08::writeTransmitRegister(TWord _val)
	{
		LOG("HDI08 write: 0x" << HEX(_val));
		if (m_hcr&2)
		{
			m_periph.getDSP().injectInterrupt(Vba_Host_Transmit_Data_Empty);
		}
	}

};
