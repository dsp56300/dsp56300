#include "peripherals.h"

#include "hi08.h"
#include "logging.h"
#include "dsp.h"


namespace dsp56k
{
	// _____________________________________________________________________________
	// Peripherals
	//
	Peripherals56303::Peripherals56303()
		: m_mem(0x0)
		, m_essi(*this)
	{
		m_mem[XIO_IDR - XIO_Reserved_High_First] = 0x001362;
	}

	TWord Peripherals56303::read(TWord _addr)
	{
//		LOG( "Periph read @ " << std::hex << _addr );

		switch (_addr)
		{
		case HI08::HSR:
			return m_hi08.readStatusRegister();
		case HI08::HRX:
			return m_hi08.read();

		case Essi::ESSI0_RX:
			return m_essi.readRX(0);
		case Essi::ESSI0_SSISR:
			return m_essi.readSR();
		}
		return m_mem[_addr - XIO_Reserved_High_First];
	}

	void Peripherals56303::write(TWord _addr, TWord _val)
	{
//		LOG( "Periph write @ " << std::hex << _addr );

		switch (_addr)
		{
		case HI08::HSR:
			m_hi08.writeStatusRegister(_val);
			break;
		case  Essi::ESSI0_SSISR:
			m_essi.writeSR(_val);
			return;
		case  Essi::ESSI0_TX0:
			m_essi.writeTX(0, _val);
			return;
		case  Essi::ESSI0_TX1:
			m_essi.writeTX(1, _val);
			return;
		case  Essi::ESSI0_TX2:
			m_essi.writeTX(2, _val);
			return;
		default:
			m_mem[_addr - XIO_Reserved_High_First] = _val;
		}
	}

	void Peripherals56303::exec()
	{
		m_essi.exec();
	}

	void Peripherals56303::reset()
	{
		m_essi.reset();
		m_hi08.reset();
	}

	Peripherals56362::Peripherals56362() : m_mem(0), m_esai(*this), m_hdi08(*this), m_wasReset(false)
	{
	}

	TWord Peripherals56362::read(TWord _addr)
	{
		switch (_addr)
		{
		case HDI08::HSR:
			return m_hdi08.readStatusRegister();
		case HDI08::HCR:
			return m_hdi08.readControlRegister();
		case HDI08::HRX:
			return m_hdi08.read();

		case Esai::M_RCR:
			return m_esai.readReceiveControlRegister();
		case Esai::M_SAISR:
			return m_esai.readStatusRegister();
		case Esai::M_TCR:
			return m_esai.readTransmitControlRegister();
		case Esai::M_RX0:
		case Esai::M_RX1:
		case Esai::M_RX2:
		case Esai::M_RX3:
			LOG("ESAI READ");
			return 0;
		case 0xFFFFBE:	// Port C Direction Register
			return 0;

		case 0xffffff:
		case 0xfffffe:
			return m_mem[_addr - XIO_Reserved_High_First];
		case 0xFFFF93:			// SHI__HTX
		case 0xFFFF94:			// SHI__HRX
			LOG("Read from " << HEX(_addr));
			return 0;	//m_mem[_addr - XIO_Reserved_High_First];	// There is nothing connected.

		case 0xFFFFF5:					// ID Register
			return 0x362;
		}

		LOG( "Periph read @ " << std::hex << _addr << ": returning (0x" <<  m_mem[_addr - XIO_Reserved_High_First] << ")");

		return m_mem[_addr - XIO_Reserved_High_First];
	}

	void Peripherals56362::write(TWord _addr, TWord _val)
	{
		switch (_addr)
		{
		case HDI08::HSR:
			m_hdi08.writeStatusRegister(_val);
			break;
		case HDI08::HCR:
			m_hdi08.writeControlRegister(_val);
			break;
		case HDI08::HTX:
			m_hdi08.writeTransmitRegister(_val);
			break;
				
		case 0xFFFF86:	// TLR2
		case 0xffff87:	// TCSR2
		case 0xffff8a:	// TLR1
		case 0xffff8b:	// TCSR1
			LOG("Timer register " << HEX(_addr) << ": " << HEX(_val));
			return;

		case 0xFFFF8F:	// TCSR0
			if (!(m_mem[_addr - XIO_Reserved_High_First] & 1) && (_val & 1) )
			{
				m_mem[0xFFFF8C - XIO_Reserved_High_First] = m_mem[0xFFFF8E - XIO_Reserved_High_First];
			}
		case 0xFFFF8D:	// TCPR0
		case 0xFFFF8E:	// TLR0
			LOG("Timer register " << HEX(_addr) << ": " << HEX(_val));
			break;

			
		case 0xFFFF93:			// SHI__HTX
		case 0xFFFF94:			// SHI__HRX
			LOG("Write to " << HEX(_addr) << ": " << HEX(_val));
//			m_mem[_addr - XIO_Reserved_High_First] = _val;	// Do not write!
			return;

		case Esai::M_RCR:
			m_esai.writeReceiveControlRegister(_val);
			return;
		case Esai::M_TCR:
			m_esai.writeTransmitControlRegister(_val);
			return;
		case Esai::M_TCCR:
			m_esai.writeTransmitControlRegister(_val);
			return;
		case Esai::M_SAICR:
			m_esai.writeCommonControlRegister(_val);
			return;
		case Esai::M_RCCR:
			m_esai.writeReceiveClockControlRegister(_val);
			return;
		case Esai::M_TX0:
		case Esai::M_TX1:
		case Esai::M_TX2:
			LOG("ESAI WRITE");
			return;
		default:
			break;
		}
		LOG( "Periph write @ " << std::hex << _addr << ": 0x" << HEX(_val));
		m_mem[_addr - XIO_Reserved_High_First] = _val;
	}

	void Peripherals56362::exec()
	{
		m_esai.exec();
		m_hdi08.exec();
		TWord TCSR0 = m_mem[0xFFFF8F - XIO_Reserved_High_First];
		if (TCSR0 & 1)
		{
			TWord TCR = m_mem[0xFFFF8C - XIO_Reserved_High_First];
			TCR++;
			TCR=TCR&0xFFFFFF;
			m_mem[0xFFFF8C - XIO_Reserved_High_First] = TCR;
			if (TCR == m_mem[0xFFFF8D - XIO_Reserved_High_First] && (TCSR0 & 4))
			{
				getDSP().injectInterrupt(InterruptVectorAddress56362::Vba_TIMER0_Compare);
			}
			if (!TCR && (TCSR0 & 2))
			{
				getDSP().injectInterrupt(InterruptVectorAddress56362::Vba_TIMER0_Overflow);
			}
			if (!TCR)
			{
				m_mem[0xFFFF8C - XIO_Reserved_High_First] = m_mem[0xFFFF8E - XIO_Reserved_High_First];
			}
		}
	}

	void Peripherals56362::reset()
	{
		m_wasReset = true;
	}
}
