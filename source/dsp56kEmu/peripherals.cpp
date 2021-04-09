#include "peripherals.h"

#include "hi08.h"
#include "logging.h"

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

	Peripherals56362::Peripherals56362() : m_mem(0), m_esai(*this)
	{
	}

	TWord Peripherals56362::read(TWord _addr)
	{
		LOG( "Periph read @ " << std::hex << _addr );

		switch (_addr)
		{
		case HI08::HSR:
			return m_hi08.readStatusRegister();
		case HI08::HRX:
			return m_hi08.read();
		case Esai::M_SAISR:
			return m_esai.readStatusRegister();
		case Esai::M_TCR:
			return m_esai.readTransmitControlRegister();
		}
		return m_mem[_addr - XIO_Reserved_High_First];
	}

	void Peripherals56362::write(TWord _addr, TWord _val)
	{
		LOG( "Periph write @ " << std::hex << _addr );

		switch (_addr)
		{
		case HI08::HSR:
			m_hi08.writeStatusRegister(_val);
			break;
		case Esai::M_TCR:
			m_esai.writeTransmitControlRegister(_val);
			return;
/*
		case  Esai::M_TCCR:
			m_esai.writeSR(_val);
			return;
		case  Essi::ESSI0_TX0:
			m_esai.writeTX(0, _val);
			return;
		case  Essi::ESSI0_TX1:
			m_essi.writeTX(1, _val);
			return;
		case  Essi::ESSI0_TX2:
			m_esai.writeTX(2, _val);
			return;
		default:
*/
		}
		m_mem[_addr - XIO_Reserved_High_First] = _val;
	}

	void Peripherals56362::exec()
	{
		m_esai.exec();
	}

	void Peripherals56362::reset()
	{
	}
}
