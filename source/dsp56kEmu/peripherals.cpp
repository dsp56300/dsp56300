#include "peripherals.h"

#include "aar.h"
#include "disasm.h"
#include "dsp.h"
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

	Peripherals56362::Peripherals56362() : m_mem(0), m_esai(*this), m_hdi08(*this), m_timers(*this), m_disableTimers(false)
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
			return m_esai.readRX(_addr - Esai::M_RX0);
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

		case M_AAR0:
		case M_AAR1:
		case M_AAR2:
		case M_AAR3:
			return m_mem[_addr - XIO_Reserved_High_First];
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
		case HDI08::HPCR:
			m_hdi08.writePortControlRegister(_val);
			return;
		case HDI08::HOTX:
			m_hdi08.writeTX(_val);
			return;

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
		
		case 0xFFFF91:			// SHI__HCSR
				if (!_val) m_disableTimers=true;		// TODO: HACK to disable timers once we don't need them anymore
				return;
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
		if (!m_disableTimers) m_timers.exec();
	}

	void Peripherals56362::reset()
	{
	}

	void Peripherals56362::setSymbols(Disassembler& _disasm)
	{
		constexpr std::pair<int,const char*> symbols[] =
		{
			// ESAI
			{Esai::M_RSMB	,	"M_RSMB"},
			{Esai::M_RSMA	,	"M_RSMA"},
			{Esai::M_TSMB	,	"M_TSMB"},
			{Esai::M_TSMA	,	"M_TSMA"},
			{Esai::M_RCCR	,	"M_RCCR"},
			{Esai::M_RCR	,	"M_RCR"},
			{Esai::M_TCCR	,	"M_TCCR"},
			{Esai::M_TCR	,	"M_TCR"},
			{Esai::M_SAICR	,	"M_SAICR"},
			{Esai::M_SAISR	,	"M_SAISR"},
			{Esai::M_RX3	,	"M_RX3"},
			{Esai::M_RX2	,	"M_RX2"},
			{Esai::M_RX1	,	"M_RX1"},
			{Esai::M_RX0	,	"M_RX0"},
			{Esai::M_TSR	,	"M_TSR"},
			{Esai::M_TX5	,	"M_TX5"},
			{Esai::M_TX4	,	"M_TX4"},
			{Esai::M_TX3	,	"M_TX3"},
			{Esai::M_TX2	,	"M_TX2"},
			{Esai::M_TX1	,	"M_TX1"},
			{Esai::M_TX0	,	"M_TX0"},

			// Timers
			{Timers::M_TCSR0, "M_TCSR0"},
			{Timers::M_TLR0	, "M_TLR0"},
			{Timers::M_TCPR0, "M_TCPR0"},
			{Timers::M_TCR0	, "M_TCR0"},
			{Timers::M_TCSR1, "M_TCSR1"},
			{Timers::M_TLR1	, "M_TLR1"},
			{Timers::M_TCPR1, "M_TCPR1"},
			{Timers::M_TCR1	, "M_TCR1"},
			{Timers::M_TCSR2, "M_TCSR2"},
			{Timers::M_TLR2	, "M_TLR2"},
			{Timers::M_TCPR2, "M_TCPR2"},
			{Timers::M_TCR2	, "M_TCR2"},
			{Timers::M_TPLR	, "M_TPLR"},
			{Timers::M_TPCR	, "M_TPCR"},

			// HDI08
			{HDI08::HCR	, "M_HCR"},
			{HDI08::HSR	, "M_HSR"},
			{HDI08::HPCR, "M_HPCR"},
			{HDI08::HORX, "M_HORX"},
			{HDI08::HOTX, "M_HOTX"},
			{HDI08::HDDR, "M_HDDR"},
			{HDI08::HDR, "M_HDR"},

			// AAR
			{M_AAR0, "M_AAR0"},
			{M_AAR1, "M_AAR1"},
			{M_AAR2, "M_AAR2"},
			{M_AAR3, "M_AAR3"},

			// Others
			{XIO_DCR5, "M_DCR5"},
			{XIO_DCO5, "M_DCO5"},
			{XIO_DDR5, "M_DDR5"},
			{XIO_DSR5, "M_DSR5"},
			{XIO_DCR4, "M_DCR4"},
			{XIO_DCO4, "M_DCO4"},
			{XIO_DDR4, "M_DDR4"},
			{XIO_DSR4, "M_DSR4"},
			{XIO_DCR3, "M_DCR3"},
			{XIO_DCO3, "M_DCO3"},
			{XIO_DDR3, "M_DDR3"},
			{XIO_DSR3, "M_DSR3"},
			{XIO_DCR2, "M_DCR2"},
			{XIO_DCO2, "M_DCO2"},
			{XIO_DDR2, "M_DDR2"},
			{XIO_DSR2, "M_DSR2"},
			{XIO_DCR1, "M_DCR1"},
			{XIO_DCO1, "M_DCO1"},
			{XIO_DDR1, "M_DDR1"},
			{XIO_DSR1, "M_DSR1"},
			{XIO_DCR0, "M_DCR0"},
			{XIO_DCO0, "M_DCO0"},
			{XIO_DDR0, "M_DDR0"},
			{XIO_DSR0, "M_DSR0"},
			{XIO_DOR3, "M_DOR3"},
			{XIO_DOR2, "M_DOR2"},
			{XIO_DOR1, "M_DOR1"},
			{XIO_DOR0, "M_DOR0"},
			{XIO_DSTR, "M_DSTR"},
			{XIO_IDR,  "M_IDR"},
			{XIO_AAR3, "M_AAR3"},
			{XIO_AAR2, "M_AAR2"},
			{XIO_AAR1, "M_AAR1"},
			{XIO_AAR0, "M_AAR0"},
			{XIO_DCR,  "M_DCR"},
			{XIO_BCR,  "M_BCR"},
			{XIO_OGDB, "M_OGDB"},
			{XIO_PCTL, "M_PCTL"},
			{XIO_IPRP, "M_IPRP"},
			{XIO_IPRC, "M_IPRC"},

			{0xFFFF90, "M_HCKR"},	// SHI Clock Control Register (HCKR)
			{0xFFFF91, "M_HCSR"},	// SHI Control/Status Register (HCSR)
			{0xFFFFF5, "M_ID"}
		};

		for (const auto& symbol : symbols)
		{
			_disasm.addSymbol(Disassembler::MemX, symbol.first, symbol.second);	
		}
	}

	void Peripherals56362::terminate()
	{
		m_hdi08.terminate();

		m_esai.terminate();
	}
}
