#include "dsp.h"
#include "interrupts.h"
#include "hi08.h"

namespace dsp56k
{
	void HI08::exec()
	{
		if (m_hcr&1 && !m_data.empty())
		{
			m_periph.getDSP().injectInterrupt(Vba_Host_Receive_Data_Full);
		}
	}

	void HI08::writeTransmitRegister(TWord _val)
	{
		LOG("HI08 write: 0x" << HEX(_val));
		if (m_hcr&2)
		{
			m_periph.getDSP().injectInterrupt(Vba_Host_Transmit_Data_Empty);
		}
	}

};
