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

	Peripherals56362::Peripherals56362() : m_mem(0), m_esai(*this), m_hdi08(*this), m_timers(*this)
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
		case HDI08::HPCR:
			return m_hdi08.readPortControlRegister();
		case HDI08::HORX:
			return m_hdi08.readRX();

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
			m_esai.readRX(_addr - Esai::M_RX0);
			return 0;
		case 0xFFFFBE:	// Port C Direction Register
			return 0;

			
		case Timers::M_TCSR0:		return m_timers.readTCSR(0);	// TIMER0 Control/Status Register
		case Timers::M_TCSR1:		return m_timers.readTCSR(1);	// TIMER1 Control/Status Register
		case Timers::M_TCSR2:		return m_timers.readTCSR(2);	// TIMER2 Control/Status Register
		case Timers::M_TLR0:		return m_timers.readTLR	(0);	// TIMER0 Load Reg
		case Timers::M_TLR1:		return m_timers.readTLR	(1);	// TIMER1 Load Reg
		case Timers::M_TLR2:		return m_timers.readTLR	(2);	// TIMER2 Load Reg
		case Timers::M_TCPR0:		return m_timers.readTCPR(0);	// TIMER0 Compare Register
		case Timers::M_TCPR1:		return m_timers.readTCPR(1);	// TIMER1 Compare Register
		case Timers::M_TCPR2:		return m_timers.readTCPR(2);	// TIMER2 Compare Register
		case Timers::M_TCR0:		return m_timers.readTCR	(0);	// TIMER0 Count Register
		case Timers::M_TCR1:		return m_timers.readTCR	(1);	// TIMER1 Count Register
		case Timers::M_TCR2:		return m_timers.readTCR	(2);	// TIMER2 Count Register

		case Timers::M_TPLR:		return m_timers.readTPLR();		// TIMER Prescaler Load Register
		case Timers::M_TPCR:		return m_timers.readTPCR();		// TIMER Prescalar Count Register

		case 0xffffff:
		case 0xfffffe:
			return m_mem[_addr - XIO_Reserved_High_First];
		case 0xFFFF93:			// SHI__HTX
		case 0xFFFF94:			// SHI__HRX
//			LOG("Read from " << HEX(_addr));
			return 0;	//m_mem[_addr - XIO_Reserved_High_First];	// There is nothing connected.

		case 0xFFFFF4:					// DMA status reg
			return 0x3f;
		case 0xFFFFF5:					// ID Register
			return 0x362;
		}

		auto& value = m_mem[_addr - XIO_Reserved_High_First];

		if (_addr!=0xffffd5) {LOG( "Periph read @ " << std::hex << _addr << ": returning (0x" <<  HEX(value) << ")");}

		return value;
	}

	void Peripherals56362::write(TWord _addr, TWord _val)
	{
		switch (_addr)
		{
		case HDI08::HSR:
			m_hdi08.writeStatusRegister(_val);
			return;
		case HDI08::HCR:
			m_hdi08.writeControlRegister(_val);
			return;
			break;
		case HDI08::HPCR:
			m_hdi08.writePortControlRegister(_val);
			return;
		case HDI08::HOTX:
			m_hdi08.writeTX(_val);

		case Timers::M_TCSR0:		m_timers.writeTCSR	(0, _val);	return;		// TIMER0 Control/Status Register
		case Timers::M_TCSR1:		m_timers.writeTCSR	(1, _val);	return;		// TIMER1 Control/Status Register
		case Timers::M_TCSR2:		m_timers.writeTCSR	(2, _val);	return;		// TIMER2 Control/Status Register
		case Timers::M_TLR0:		m_timers.writeTLR	(0, _val);	return;		// TIMER0 Load Reg
		case Timers::M_TLR1:		m_timers.writeTLR	(1, _val);	return;		// TIMER1 Load Reg
		case Timers::M_TLR2:		m_timers.writeTLR	(2, _val);	return;		// TIMER2 Load Reg
		case Timers::M_TCPR0:		m_timers.writeTCPR	(0, _val);	return;		// TIMER0 Compare Register
		case Timers::M_TCPR1:		m_timers.writeTCPR	(1, _val);	return;		// TIMER1 Compare Register
		case Timers::M_TCPR2:		m_timers.writeTCPR	(2, _val);	return;		// TIMER2 Compare Register
		case Timers::M_TCR0:		m_timers.writeTCR	(0, _val);	return;		// TIMER0 Count Register
		case Timers::M_TCR1:		m_timers.writeTCR	(1, _val);	return;		// TIMER1 Count Register
		case Timers::M_TCR2:		m_timers.writeTCR	(2, _val);	return;		// TIMER2 Count Register

		case Timers::M_TPLR:		m_timers.writeTPLR	(_val);		return;		// TIMER Prescaler Load Register
		case Timers::M_TPCR:		m_timers.writeTPCR	(_val);		return;		// TIMER Prescalar Count Register
			
		case 0xFFFF93:			// SHI__HTX
		case 0xFFFF94:			// SHI__HRX
//			LOG("Write to " << HEX(_addr) << ": " << HEX(_val));
//			m_mem[_addr - XIO_Reserved_High_First] = _val;	// Do not write!
			return;

		case Esai::M_SAISR:
			m_esai.writestatusRegister(_val);
			return;
		case Esai::M_SAICR:
			m_esai.writeControlRegister(_val);
			return;
		case Esai::M_RCR:
			m_esai.writeReceiveControlRegister(_val);
			return;
		case Esai::M_RCCR:
			m_esai.writeReceiveClockControlRegister(_val);
			return;
		case Esai::M_TCR:
			m_esai.writeTransmitControlRegister(_val);
			return;
		case Esai::M_TCCR:
			m_esai.writeTransmitClockControlRegister(_val);
			return;
		case Esai::M_TX0:
		case Esai::M_TX1:
		case Esai::M_TX2:
		case Esai::M_TX3:
		case Esai::M_TX4:
		case Esai::M_TX5:
			m_esai.writeTX(_addr - Esai::M_TX0, _val);
			return;
		
		case 0xFFFFFD:	m_esai.updatePCTL(_val);
			return;
		default:
			break;
		}
		if (_addr!=0xffffd5) {LOG( "Periph write @ " << std::hex << _addr << ": 0x" << HEX(_val));}
		m_mem[_addr - XIO_Reserved_High_First] = _val;
	}

	void Peripherals56362::exec()
	{
		m_esai.exec();
		m_hdi08.exec();
		m_timers.exec();
	}

	void Peripherals56362::reset()
	{
	}
}
