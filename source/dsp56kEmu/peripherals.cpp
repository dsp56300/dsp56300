#include "peripherals.h"

#include "aar.h"
#include "disasm.h"
#include "dsp.h"
#include "interrupts.h"
#include "logging.h"

namespace dsp56k
{
	namespace
	{
		void setSymbolsX(Disassembler& _disasm)
		{
			constexpr std::pair<int,const char*> symbols[] =
			{
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
			};

			for (const auto& symbol : symbols)
				_disasm.addSymbol(Disassembler::MemX, symbol.first, symbol.second);

			// AAR bits
			constexpr std::pair<TWord, const char*> aarBitSymbols[] =
			{
				{M_BAT  , "M_BAT"},
				{1<<M_BAT0 , "M_BAT0"},
				{1<<M_BAT1 , "M_BAT1"},
				{1<<M_BAAP , "M_BAAP"},
				{1<<M_BPEN , "M_BPEN"},
				{1<<M_BXEN , "M_BXEN"},
				{1<<M_BYEN , "M_BYEN"},
				{1<<M_BAM  , "M_BAM"},
				{1<<M_BPAC , "M_BPAC"},
				{M_BNC  , "M_BNC"},
				{1<<M_BNC0 , "M_BNC0"},
				{1<<M_BNC1 , "M_BNC1"},
				{1<<M_BNC2 , "M_BNC2"},
				{1<<M_BNC3 , "M_BNC3"},
				{M_BAC  , "M_BAC"},
				{1<<M_BAC0 , "M_BAC0"},
				{1<<M_BAC1 , "M_BAC1"},
				{1<<M_BAC2 , "M_BAC2"},
				{1<<M_BAC3 , "M_BAC3"},
				{1<<M_BAC4 , "M_BAC4"},
				{1<<M_BAC5 , "M_BAC5"},
				{1<<M_BAC6 , "M_BAC6"},
				{1<<M_BAC7 , "M_BAC7"},
				{1<<M_BAC8 , "M_BAC8"},
				{1<<M_BAC9 , "M_BAC9"},
				{1<<M_BAC10, "M_BAC10"},
				{1<<M_BAC11, "M_BAC11"}
			};

			for(TWord a = M_AAR3; a <= M_AAR0; ++a)
			{
				for (const auto& aarBitSymbol : aarBitSymbols)
				{
					_disasm.addBitMaskSymbol(Disassembler::MemX, a, aarBitSymbol.first, aarBitSymbol.second);
				}
			}
		}
	}

	// _____________________________________________________________________________
	// Peripherals
	//
	Peripherals56303::Peripherals56303()
		: m_mem(0x0)
		, m_dma(*this)
		, m_essiClock(*this)
		, m_essi0(*this, 0)
		, m_essi1(*this, 1)
		, m_hi08(*this)
		, m_timers(*this, Vba_TIMER0compare)
	{
		m_essiClock.setEsaiDivider(&m_essi0, 0);
		m_essiClock.setEsaiDivider(&m_essi1, 0);
		m_essiClock.setExternalClockFrequency(4000000);			// 4 MHz

		m_mem[XIO_IDR - XIO_Reserved_High_First] = 0x0005303;	// rev 5 derivative number 303
	}

	TWord Peripherals56303::read(TWord _addr, Instruction _inst)
	{
		switch (_addr)
		{
		case HDI08::HSR:			return m_hi08.readStatusRegister();
		case HDI08::HCR:			return m_hi08.readControlRegister();
		case HDI08::HPCR:			return m_hi08.readPortControlRegister();
		case HDI08::HORX:			return m_hi08.readRX(_inst);
		case HDI08::HDR:			return m_hi08.readHDR();
		case HDI08::HDDR:			return m_hi08.readHDDR();

		case Essi::ESSI0_TX0:		return m_essi0.readTX(0);
		case Essi::ESSI0_TX1:		return m_essi0.readTX(1);
		case Essi::ESSI0_TX2:		return m_essi0.readTX(2);
		case Essi::ESSI0_TSR:		return m_essi0.readTSR();
		case Essi::ESSI0_RX:		return m_essi0.readRX();
		case Essi::ESSI0_SSISR:		return m_essi0.readSR();
		case Essi::ESSI0_CRA:		return m_essi0.readCRA();
		case Essi::ESSI0_CRB:		return m_essi0.readCRB();
		case Essi::ESSI0_TSMA:		return m_essi0.readTSMA();
		case Essi::ESSI0_TSMB:		return m_essi0.readTSMB();
		case Essi::ESSI0_RSMA:		return m_essi0.readRSMA();
		case Essi::ESSI0_RSMB:		return m_essi0.readRSMB();

		case Essi::ESSI1_TX0:		return m_essi1.readTX(0);
		case Essi::ESSI1_TX1:		return m_essi1.readTX(1);
		case Essi::ESSI1_TX2:		return m_essi1.readTX(2);
		case Essi::ESSI1_TSR:		return m_essi1.readTSR();
		case Essi::ESSI1_RX:		return m_essi1.readRX();
		case Essi::ESSI1_SSISR:		return m_essi1.readSR();
		case Essi::ESSI1_CRA:		return m_essi1.readCRA();
		case Essi::ESSI1_CRB:		return m_essi1.readCRB();
		case Essi::ESSI1_TSMA:		return m_essi1.readTSMA();
		case Essi::ESSI1_TSMB:		return m_essi1.readTSMB();
		case Essi::ESSI1_RSMA:		return m_essi1.readRSMA();
		case Essi::ESSI1_RSMB:		return m_essi1.readRSMB();

		case XIO_PCTL:				return m_essiClock.getPCTL();

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

		case XIO_DCR5: return m_dma.getDCR(5);	// DMA 5 Control Register
		case XIO_DCO5: return m_dma.getDCO(5);	// DMA 5 Counter
		case XIO_DDR5: return m_dma.getDDR(5);	// DMA 5 Destination Address Register
		case XIO_DSR5: return m_dma.getDSR(5);	// DMA 5 Source Address Register

		case XIO_DCR4: return m_dma.getDCR(4);	// DMA 4 Control Register
		case XIO_DCO4: return m_dma.getDCO(4);	// DMA 4 Counter
		case XIO_DDR4: return m_dma.getDDR(4);	// DMA 4 Destination Address Register
		case XIO_DSR4: return m_dma.getDSR(4);	// DMA 4 Source Address Register

		case XIO_DCR3: return m_dma.getDCR(3);	// DMA 3 Control Register
		case XIO_DCO3: return m_dma.getDCO(3);	// DMA 3 Counter
		case XIO_DDR3: return m_dma.getDDR(3);	// DMA 3 Destination Address Register
		case XIO_DSR3: return m_dma.getDSR(3);	// DMA 3 Source Address Register

		case XIO_DCR2: return m_dma.getDCR(2);	// DMA 2 Control Register
		case XIO_DCO2: return m_dma.getDCO(2);	// DMA 2 Counter
		case XIO_DDR2: return m_dma.getDDR(2);	// DMA 2 Destination Address Register
		case XIO_DSR2: return m_dma.getDSR(2);	// DMA 2 Source Address Register

		case XIO_DCR1: return m_dma.getDCR(1);	// DMA 1 Control Register
		case XIO_DCO1: return m_dma.getDCO(1);	// DMA 1 Counter
		case XIO_DDR1: return m_dma.getDDR(1);	// DMA 1 Destination Address Register
		case XIO_DSR1: return m_dma.getDSR(1);	// DMA 1 Source Address Register

		case XIO_DCR0: return m_dma.getDCR(0);	// DMA 0 Control Register
		case XIO_DCO0: return m_dma.getDCO(0);	// DMA 0 Counter
		case XIO_DDR0: return m_dma.getDDR(0);	// DMA 0 Destination Address Register
		case XIO_DSR0: return m_dma.getDSR(0);	// DMA 0 Source Address Register

		case XIO_DOR3: return m_dma.getDOR(3);	// DMA Offset Register 3
		case XIO_DOR2: return m_dma.getDOR(2);	// DMA Offset Register 2 
		case XIO_DOR1: return m_dma.getDOR(1);	// DMA Offset Register 1
		case XIO_DOR0: return m_dma.getDOR(0);	// DMA Offset Register 0

		case XIO_DSTR: return m_dma.getDSTR();	// DMA Status Register
		}

//		LOG( "Periph read @ " << std::hex << _addr );

		return m_mem[_addr - XIO_Reserved_High_First];
	}

	const TWord* Peripherals56303::readAsPtr(const TWord _addr, Instruction _inst)
	{
		switch (_addr)
		{
		case Essi::ESSI0_SSISR:		return &m_essi0.readSR();
		case Essi::ESSI1_SSISR:		return &m_essi1.readSR();
		default:					return nullptr;
		}
	}

	void Peripherals56303::write(TWord _addr, TWord _val)
	{
		switch (_addr)
		{
		case HDI08::HSR:			m_hi08.writeStatusRegister(_val);		return;
		case HDI08::HCR:			m_hi08.writeControlRegister(_val);		return;
		case HDI08::HPCR:			m_hi08.writePortControlRegister(_val);	return;
		case HDI08::HOTX:			m_hi08.writeTX(_val);					return;
		case HDI08::HDR:			m_hi08.writeHDR(_val);					return;
		case HDI08::HDDR:			m_hi08.writeHDDR(_val);					return;


		case Essi::ESSI0_TX0:		m_essi0.writeTX(0, _val);				return;
		case Essi::ESSI0_TX1:		m_essi0.writeTX(1, _val);				return;
		case Essi::ESSI0_TX2:		m_essi0.writeTX(2, _val);				return;
		case Essi::ESSI0_TSR:		m_essi0.writeTSR(_val);					return;
		case Essi::ESSI0_RX:		m_essi0.writeRX(_val);					return;
		case Essi::ESSI0_SSISR:		m_essi0.writeSR(_val);					return;
		case Essi::ESSI0_CRA:		m_essi0.writeCRA(_val);					return;
		case Essi::ESSI0_CRB:		m_essi0.writeCRB(_val);					return;
		case Essi::ESSI0_TSMA:		m_essi0.writeTSMA(_val);				return;
		case Essi::ESSI0_TSMB:		m_essi0.writeTSMB(_val);				return;
		case Essi::ESSI0_RSMA:		m_essi0.writeRSMA(_val);				return;
		case Essi::ESSI0_RSMB:		m_essi0.writeRSMB(_val);				return;

		case Essi::ESSI1_TX0:		m_essi1.writeTX(0, _val);				return;
		case Essi::ESSI1_TX1:		m_essi1.writeTX(1, _val);				return;
		case Essi::ESSI1_TX2:		m_essi1.writeTX(2, _val);				return;
		case Essi::ESSI1_TSR:		m_essi1.writeTSR(_val);					return;
		case Essi::ESSI1_RX:		m_essi1.writeRX(_val);					return;
		case Essi::ESSI1_SSISR:		m_essi1.writeSR(_val);					return;
		case Essi::ESSI1_CRA:		m_essi1.writeCRA(_val);					return;
		case Essi::ESSI1_CRB:		m_essi1.writeCRB(_val);					return;
		case Essi::ESSI1_TSMA:		m_essi1.writeTSMA(_val);				return;
		case Essi::ESSI1_TSMB:		m_essi1.writeTSMB(_val);				return;
		case Essi::ESSI1_RSMA:		m_essi1.writeRSMA(_val);				return;
		case Essi::ESSI1_RSMB:		m_essi1.writeRSMB(_val);				return;

		case XIO_PCTL:				m_essiClock.setPCTL(_val);				return;

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
		

		case XIO_DCR5: m_dma.setDCR(5, _val); return;	// DMA 5 Control Register
		case XIO_DCO5: m_dma.setDCO(5, _val); return;	// DMA 5 Counter
		case XIO_DDR5: m_dma.setDDR(5, _val); return;	// DMA 5 Destination Address Register
		case XIO_DSR5: m_dma.setDSR(5, _val); return;	// DMA 5 Source Address Register

		case XIO_DCR4: m_dma.setDCR(4, _val); return;	// DMA 4 Control Register
		case XIO_DCO4: m_dma.setDCO(4, _val); return;	// DMA 4 Counter
		case XIO_DDR4: m_dma.setDDR(4, _val); return;	// DMA 4 Destination Address Register
		case XIO_DSR4: m_dma.setDSR(4, _val); return;	// DMA 4 Source Address Register

		case XIO_DCR3: m_dma.setDCR(3, _val); return;	// DMA 3 Control Register
		case XIO_DCO3: m_dma.setDCO(3, _val); return;	// DMA 3 Counter
		case XIO_DDR3: m_dma.setDDR(3, _val); return;	// DMA 3 Destination Address Register
		case XIO_DSR3: m_dma.setDSR(3, _val); return;	// DMA 3 Source Address Register

		case XIO_DCR2: m_dma.setDCR(2, _val); return;	// DMA 2 Control Register
		case XIO_DCO2: m_dma.setDCO(2, _val); return;	// DMA 2 Counter
		case XIO_DDR2: m_dma.setDDR(2, _val); return;	// DMA 2 Destination Address Register
		case XIO_DSR2: m_dma.setDSR(2, _val); return;	// DMA 2 Source Address Register

		case XIO_DCR1: m_dma.setDCR(1, _val); return;	// DMA 1 Control Register
		case XIO_DCO1: m_dma.setDCO(1, _val); return;	// DMA 1 Counter
		case XIO_DDR1: m_dma.setDDR(1, _val); return;	// DMA 1 Destination Address Register
		case XIO_DSR1: m_dma.setDSR(1, _val); return;	// DMA 1 Source Address Register

		case XIO_DCR0: m_dma.setDCR(0, _val); return;	// DMA 0 Control Register
		case XIO_DCO0: m_dma.setDCO(0, _val); return;	// DMA 0 Counter
		case XIO_DDR0: m_dma.setDDR(0, _val); return;	// DMA 0 Destination Address Register
		case XIO_DSR0: m_dma.setDSR(0, _val); return;	// DMA 0 Source Address Register

		case XIO_DOR3: m_dma.setDOR(3, _val); return;	// DMA Offset Register 3
		case XIO_DOR2: m_dma.setDOR(2, _val); return;	// DMA Offset Register 2 
		case XIO_DOR1: m_dma.setDOR(1, _val); return;	// DMA Offset Register 1
		case XIO_DOR0: m_dma.setDOR(0, _val); return;	// DMA Offset Register 0

//		case XIO_DSTR: m_dma.setDSTR(_val); return;		// DMA Status Register is read only
		default:
//			LOG( "Periph write @ " << std::hex << _addr );
			m_mem[_addr - XIO_Reserved_High_First] = _val;
		}
	}

	void Peripherals56303::exec()
	{
		m_essiClock.exec();
		m_hi08.exec();
		m_timers.exec();
		m_dma.exec();
	}

	void Peripherals56303::reset()
	{
		m_essi0.reset();
		m_essi1.reset();
		m_hi08.reset();
	}

	void Peripherals56303::setSymbols(Disassembler& _disasm) const
	{
		m_essi0.setSymbols(_disasm);
		m_essi1.setSymbols(_disasm);
		HDI08::setSymbols(_disasm);
		m_timers.setSymbols(_disasm);
		setSymbolsX(_disasm);
	}

	void Peripherals56303::terminate()
	{
		m_hi08.terminate();
		m_essi0.terminate();
		m_essi1.terminate();
	}

	Peripherals56362::Peripherals56362(Peripherals56367* _peripherals56367/* = nullptr*/)
	: m_mem(0)
	, m_dma(*this)
	, m_esaiClock(*this)
	, m_esai(*this, MemArea_X, &m_dma)
	, m_hdi08(*this)
	, m_timers(*this, Vba_TIMER0_Compare)
	, m_disableTimers(false)
	{
		m_esaiClock.setEsaiDivider(&m_esai, 0);
		if(_peripherals56367)
			m_esaiClock.setEsaiDivider(&_peripherals56367->getEsai(), 0);
	}

	TWord Peripherals56362::read(const TWord _addr, const Instruction _inst)
	{
		switch (_addr)
		{
		case HDI08::HSR:	return m_hdi08.readStatusRegister();
		case HDI08::HCR:	return m_hdi08.readControlRegister();
		case HDI08::HPCR:	return m_hdi08.readPortControlRegister();
		case HDI08::HORX:	return m_hdi08.readRX(_inst);
		case HDI08::HDR:	return m_hdi08.readHDR();
		case HDI08::HDDR:	return m_hdi08.readHDDR();

		case Esai::M_RCR:	return m_esai.readReceiveControlRegister();
		case Esai::M_RCCR:	return m_esai.readReceiveClockControlRegister();
		case Esai::M_SAISR:	return m_esai.readStatusRegister();
		case Esai::M_TCR:	return m_esai.readTransmitControlRegister();
		case Esai::M_TCCR:	return m_esai.readTransmitClockControlRegister();
		case Esai::M_RX0:
		case Esai::M_RX1:
		case Esai::M_RX2:
		case Esai::M_RX3:	return m_esai.readRX(_addr - Esai::M_RX0);
		case Esai::M_TSMA:	return m_esai.readTSMA();
		case Esai::M_TSMB:	return m_esai.readTSMB();
		case Esai::M_PCRC:	return m_portC.getControl();
		case Esai::M_PDRC:	return m_portC.dspRead();
		case Esai::M_PRRC:	return m_portC.getDirection();

		case XIO_PCTL:		return m_esaiClock.getPCTL();

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

//		case XIO_DSTR:					// DMA status reg
//			return 0x3f;
		case XIO_IDR:					// ID Register
			return 0x362;

		case M_AAR0:
		case M_AAR1:
		case M_AAR2:
		case M_AAR3:
			return m_mem[_addr - XIO_Reserved_High_First];

		case XIO_DCR5: return m_dma.getDCR(5);	// DMA 5 Control Register
		case XIO_DCO5: return m_dma.getDCO(5);	// DMA 5 Counter
		case XIO_DDR5: return m_dma.getDDR(5);	// DMA 5 Destination Address Register
		case XIO_DSR5: return m_dma.getDSR(5);	// DMA 5 Source Address Register

		case XIO_DCR4: return m_dma.getDCR(4);	// DMA 4 Control Register
		case XIO_DCO4: return m_dma.getDCO(4);	// DMA 4 Counter
		case XIO_DDR4: return m_dma.getDDR(4);	// DMA 4 Destination Address Register
		case XIO_DSR4: return m_dma.getDSR(4);	// DMA 4 Source Address Register

		case XIO_DCR3: return m_dma.getDCR(3);	// DMA 3 Control Register
		case XIO_DCO3: return m_dma.getDCO(3);	// DMA 3 Counter
		case XIO_DDR3: return m_dma.getDDR(3);	// DMA 3 Destination Address Register
		case XIO_DSR3: return m_dma.getDSR(3);	// DMA 3 Source Address Register

		case XIO_DCR2: return m_dma.getDCR(2);	// DMA 2 Control Register
		case XIO_DCO2: return m_dma.getDCO(2);	// DMA 2 Counter
		case XIO_DDR2: return m_dma.getDDR(2);	// DMA 2 Destination Address Register
		case XIO_DSR2: return m_dma.getDSR(2);	// DMA 2 Source Address Register

		case XIO_DCR1: return m_dma.getDCR(1);	// DMA 1 Control Register
		case XIO_DCO1: return m_dma.getDCO(1);	// DMA 1 Counter
		case XIO_DDR1: return m_dma.getDDR(1);	// DMA 1 Destination Address Register
		case XIO_DSR1: return m_dma.getDSR(1);	// DMA 1 Source Address Register

		case XIO_DCR0: return m_dma.getDCR(0);	// DMA 0 Control Register
		case XIO_DCO0: return m_dma.getDCO(0);	// DMA 0 Counter
		case XIO_DDR0: return m_dma.getDDR(0);	// DMA 0 Destination Address Register
		case XIO_DSR0: return m_dma.getDSR(0);	// DMA 0 Source Address Register

		case XIO_DOR3: return m_dma.getDOR(3);	// DMA Offset Register 3
		case XIO_DOR2: return m_dma.getDOR(2);	// DMA Offset Register 2 
		case XIO_DOR1: return m_dma.getDOR(1);	// DMA Offset Register 1
		case XIO_DOR0: return m_dma.getDOR(0);	// DMA Offset Register 0

		case XIO_DSTR: return m_dma.getDSTR();	// DMA Status Register
		}

		const auto& value = m_mem[_addr - XIO_Reserved_High_First];

//		if (_addr!=0xffffd5) {LOG( "Periph read @ " << std::hex << _addr << ": returning (0x" <<  HEX(value) << ")");}

		return value;
	}

	const TWord* Peripherals56362::readAsPtr(const TWord _addr, Instruction _inst)
	{
		switch (_addr)
		{
		case Esai::M_PCRC:			return &m_portC.getControl();
		case Esai::M_PRRC:			return &m_portC.getDirection();
		case Esai::M_SAISR:			return &m_esai.readStatusRegister();

		case Timers::M_TCSR0:		return &m_timers.readTCSR(0);	// TIMER0 Control/Status Register
		case Timers::M_TCSR1:		return &m_timers.readTCSR(1);	// TIMER1 Control/Status Register
		case Timers::M_TCSR2:		return &m_timers.readTCSR(2);	// TIMER2 Control/Status Register
		case Timers::M_TLR0:		return &m_timers.readTLR(0);	// TIMER0 Load Reg
		case Timers::M_TLR1:		return &m_timers.readTLR(1);	// TIMER1 Load Reg
		case Timers::M_TLR2:		return &m_timers.readTLR(2);	// TIMER2 Load Reg
		case Timers::M_TCPR0:		return &m_timers.readTCPR(0);	// TIMER0 Compare Register
		case Timers::M_TCPR1:		return &m_timers.readTCPR(1);	// TIMER1 Compare Register
		case Timers::M_TCPR2:		return &m_timers.readTCPR(2);	// TIMER2 Compare Register
		case Timers::M_TCR0:		return &m_timers.readTCR(0);	// TIMER0 Count Register
		case Timers::M_TCR1:		return &m_timers.readTCR(1);	// TIMER1 Count Register
		case Timers::M_TCR2:		return &m_timers.readTCR(2);	// TIMER2 Count Register

		case Timers::M_TPLR:		return &m_timers.readTPLR();	// TIMER Prescaler Load Register
		case Timers::M_TPCR:		return &m_timers.readTPCR();	// TIMER Prescalar Count Register

		case XIO_DCR5:				return &m_dma.getDCR(5);		// DMA 5 Control Register
		case XIO_DCO5:				return &m_dma.getDCO(5);		// DMA 5 Counter
		case XIO_DDR5:				return &m_dma.getDDR(5);		// DMA 5 Destination Address Register
		case XIO_DSR5:				return &m_dma.getDSR(5);		// DMA 5 Source Address Register

		case XIO_DCR4:				return &m_dma.getDCR(4);		// DMA 4 Control Register
		case XIO_DCO4:				return &m_dma.getDCO(4);		// DMA 4 Counter
		case XIO_DDR4:				return &m_dma.getDDR(4);		// DMA 4 Destination Address Register
		case XIO_DSR4:				return &m_dma.getDSR(4);		// DMA 4 Source Address Register

		case XIO_DCR3:				return &m_dma.getDCR(3);		// DMA 3 Control Register
		case XIO_DCO3:				return &m_dma.getDCO(3);		// DMA 3 Counter
		case XIO_DDR3:				return &m_dma.getDDR(3);		// DMA 3 Destination Address Register
		case XIO_DSR3:				return &m_dma.getDSR(3);		// DMA 3 Source Address Register

		case XIO_DCR2:				return &m_dma.getDCR(2);		// DMA 2 Control Register
		case XIO_DCO2:				return &m_dma.getDCO(2);		// DMA 2 Counter
		case XIO_DDR2:				return &m_dma.getDDR(2);		// DMA 2 Destination Address Register
		case XIO_DSR2:				return &m_dma.getDSR(2);		// DMA 2 Source Address Register

		case XIO_DCR1:				return &m_dma.getDCR(1);		// DMA 1 Control Register
		case XIO_DCO1:				return &m_dma.getDCO(1);		// DMA 1 Counter
		case XIO_DDR1:				return &m_dma.getDDR(1);		// DMA 1 Destination Address Register
		case XIO_DSR1:				return &m_dma.getDSR(1);		// DMA 1 Source Address Register

		case XIO_DCR0:				return &m_dma.getDCR(0);		// DMA 0 Control Register
		case XIO_DCO0:				return &m_dma.getDCO(0);		// DMA 0 Counter
		case XIO_DDR0:				return &m_dma.getDDR(0);		// DMA 0 Destination Address Register
		case XIO_DSR0:				return &m_dma.getDSR(0);		// DMA 0 Source Address Register

		case XIO_DSTR:				return &m_dma.getDSTR();		// DMA Status Register
		}

		return nullptr;
	}

	void Peripherals56362::write(const TWord _addr, const TWord _val)
	{
		switch (_addr)
		{
		case HDI08::HSR:	m_hdi08.writeStatusRegister(_val);		return;
		case HDI08::HCR:	m_hdi08.writeControlRegister(_val);		return;
		case HDI08::HPCR:	m_hdi08.writePortControlRegister(_val);	return;
		case HDI08::HOTX:	m_hdi08.writeTX(_val);					return;
		case HDI08::HDR:	m_hdi08.writeHDR(_val);					return;
		case HDI08::HDDR:	m_hdi08.writeHDDR(_val);				return;

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
			return;
		case 0xFFFF93:			// SHI__HTX
		case 0xFFFF94:			// SHI__HRX
//			LOG("Write to " << HEX(_addr) << ": " << HEX(_val));
//			m_mem[_addr - XIO_Reserved_High_First] = _val;	// Do not write!
			return;

		case Esai::M_SAISR:			m_esai.writestatusRegister(_val);					return;
		case Esai::M_SAICR:			m_esai.writeControlRegister(_val);					return;
		case Esai::M_RCR:			m_esai.writeReceiveControlRegister(_val);			return;
		case Esai::M_RCCR:			m_esai.writeReceiveClockControlRegister(_val);		return;
		case Esai::M_TCR:			m_esai.writeTransmitControlRegister(_val);			return;
		case Esai::M_TCCR:			m_esai.writeTransmitClockControlRegister(_val);		return;
		case Esai::M_TX0:
		case Esai::M_TX1:
		case Esai::M_TX2:
		case Esai::M_TX3:
		case Esai::M_TX4:
		case Esai::M_TX5:			m_esai.writeTX(_addr - Esai::M_TX0, _val);			return;
		case Esai::M_TSMA:			m_esai.writeTSMA(_val);								return;
		case Esai::M_TSMB:			m_esai.writeTSMB(_val);								return;
		case Esai::M_PCRC:			m_portC.setControl(_val);							return;
		case Esai::M_PDRC:			m_portC.dspWrite(_val);								return;
		case Esai::M_PRRC:			m_portC.setDirection(_val);							return;

		case XIO_PCTL:				m_esaiClock.setPCTL(_val);							return;

		case XIO_DCR5: m_dma.setDCR(5, _val); return;	// DMA 5 Control Register
		case XIO_DCO5: m_dma.setDCO(5, _val); return;	// DMA 5 Counter
		case XIO_DDR5: m_dma.setDDR(5, _val); return;	// DMA 5 Destination Address Register
		case XIO_DSR5: m_dma.setDSR(5, _val); return;	// DMA 5 Source Address Register

		case XIO_DCR4: m_dma.setDCR(4, _val); return;	// DMA 4 Control Register
		case XIO_DCO4: m_dma.setDCO(4, _val); return;	// DMA 4 Counter
		case XIO_DDR4: m_dma.setDDR(4, _val); return;	// DMA 4 Destination Address Register
		case XIO_DSR4: m_dma.setDSR(4, _val); return;	// DMA 4 Source Address Register

		case XIO_DCR3: m_dma.setDCR(3, _val); return;	// DMA 3 Control Register
		case XIO_DCO3: m_dma.setDCO(3, _val); return;	// DMA 3 Counter
		case XIO_DDR3: m_dma.setDDR(3, _val); return;	// DMA 3 Destination Address Register
		case XIO_DSR3: m_dma.setDSR(3, _val); return;	// DMA 3 Source Address Register

		case XIO_DCR2: m_dma.setDCR(2, _val); return;	// DMA 2 Control Register
		case XIO_DCO2: m_dma.setDCO(2, _val); return;	// DMA 2 Counter
		case XIO_DDR2: m_dma.setDDR(2, _val); return;	// DMA 2 Destination Address Register
		case XIO_DSR2: m_dma.setDSR(2, _val); return;	// DMA 2 Source Address Register

		case XIO_DCR1: m_dma.setDCR(1, _val); return;	// DMA 1 Control Register
		case XIO_DCO1: m_dma.setDCO(1, _val); return;	// DMA 1 Counter
		case XIO_DDR1: m_dma.setDDR(1, _val); return;	// DMA 1 Destination Address Register
		case XIO_DSR1: m_dma.setDSR(1, _val); return;	// DMA 1 Source Address Register

		case XIO_DCR0: m_dma.setDCR(0, _val); return;	// DMA 0 Control Register
		case XIO_DCO0: m_dma.setDCO(0, _val); return;	// DMA 0 Counter
		case XIO_DDR0: m_dma.setDDR(0, _val); return;	// DMA 0 Destination Address Register
		case XIO_DSR0: m_dma.setDSR(0, _val); return;	// DMA 0 Source Address Register

		case XIO_DOR3: m_dma.setDOR(3, _val); return;	// DMA Offset Register 3
		case XIO_DOR2: m_dma.setDOR(2, _val); return;	// DMA Offset Register 2 
		case XIO_DOR1: m_dma.setDOR(1, _val); return;	// DMA Offset Register 1
		case XIO_DOR0: m_dma.setDOR(0, _val); return;	// DMA Offset Register 0

//		case XIO_DSTR: m_dma.setDSTR(_val); return;		// DMA Status Register is read only

		case 0xffffd2:	// DAX audio data register A
			return;
		default:
			break;
		}
		if (_addr!=0xffffd5)
		{
//			LOG( "Periph write @ " << std::hex << _addr << ": 0x" << HEX(_val));
		}
		m_mem[_addr - XIO_Reserved_High_First] = _val;
	}

	void Peripherals56362::exec()
	{
		m_esaiClock.exec();
		m_hdi08.exec();
		if (!m_disableTimers) m_timers.exec();
		m_dma.exec();
	}

	void Peripherals56362::reset()
	{
		m_hdi08.reset();
	}

	void Peripherals56362::setSymbols(Disassembler& _disasm) const
	{
		auto addIR = [&](TWord _addr, const std::string& _symbol)
		{
			_disasm.addSymbol(Disassembler::MemP, _addr, "int_" + _symbol);
		};

		addIR(Vba_HardwareRESET, "reset");
		addIR(Vba_Stackerror, "stackerror");
		addIR(Vba_Illegalinstruction, "illegal");
		addIR(Vba_DebugRequest, "debug");
		addIR(Vba_Trap, "trap");
		addIR(Vba_NMI, "nmi");
		addIR(Vba_Reserved0C, "reserved0C");
		addIR(Vba_Reserved0E, "reserved0E");

		addIR(Vba_IRQA, "irqA");
		addIR(Vba_IRQB, "irqB");
		addIR(Vba_IRQC, "irqC");
		addIR(Vba_IRQD, "irqD");

		addIR(Vba_DMAchannel0, "dma0");
		addIR(Vba_DMAchannel1, "dma1");
		addIR(Vba_DMAchannel2, "dma2");
		addIR(Vba_DMAchannel3, "dma3");
		addIR(Vba_DMAchannel4, "dma4");
		addIR(Vba_DMAchannel5, "dma5");

		addIR(Vba_Reserved24, "reserved24");
		addIR(Vba_Reserved26, "reserved26");
		addIR(Vba_Reserved4C, "reserved4c");
		addIR(Vba_Reserved4E, "reserved4e");
		addIR(Vba_Reserved50, "reserved50");
		addIR(Vba_Reserved52, "reserved52");
		addIR(Vba_Reserved66, "reserved66");

		Esai::setSymbols(_disasm, MemArea_X);

		HDI08::setSymbols(_disasm);

		m_timers.setSymbols(_disasm);

		setSymbolsX(_disasm);
	}

	void Peripherals56362::terminate()
	{
		m_hdi08.terminate();
		m_esai.terminate();
	}

	void Peripherals56362::setDSP(DSP* _dsp)
	{
		IPeripherals::setDSP(_dsp);

		m_esaiClock.setDSP(_dsp);
		m_esai.setDSP(_dsp);
		m_timers.setDSP(_dsp);
	}

	Peripherals56367::Peripherals56367() : m_mem(), m_esai(*this, MemArea_Y)
	{
		m_mem.fill(0);
	}

	TWord Peripherals56367::read(const TWord _addr, Instruction _inst)
	{
		switch (_addr)
		{
		case Esai::M_RCR_1:			return m_esai.readReceiveControlRegister();
		case Esai::M_RCCR_1:		return m_esai.readReceiveClockControlRegister();
		case Esai::M_SAISR_1:		return m_esai.readStatusRegister();
		case Esai::M_TCR_1:			return m_esai.readTransmitControlRegister();
		case Esai::M_TCCR_1:		return m_esai.readTransmitClockControlRegister();
		case Esai::M_RX0_1:
		case Esai::M_RX1_1:
		case Esai::M_RX2_1:
		case Esai::M_RX3_1:			return m_esai.readRX(_addr - Esai::M_RX0_1);
		case Esai::M_TSMA_1:		return m_esai.readTSMA();
		case Esai::M_TSMB_1:		return m_esai.readTSMB();
		default:
			return m_mem[_addr - XIO_Reserved_High_First];
		}
	}

	void Peripherals56367::write(TWord _addr, TWord _val)
	{
		switch (_addr)
		{
		case Esai::M_SAISR_1:	m_esai.writestatusRegister(_val);					return;
		case Esai::M_SAICR_1:	m_esai.writeControlRegister(_val);					return;
		case Esai::M_RCR_1:		m_esai.writeReceiveControlRegister(_val);			return;
		case Esai::M_RCCR_1:	m_esai.writeReceiveClockControlRegister(_val);		return;
		case Esai::M_TCR_1:		m_esai.writeTransmitControlRegister(_val);			return;
		case Esai::M_TCCR_1:	m_esai.writeTransmitClockControlRegister(_val);		return;
		case Esai::M_TX0_1:
		case Esai::M_TX1_1:
		case Esai::M_TX2_1:
		case Esai::M_TX3_1:
		case Esai::M_TX4_1:
		case Esai::M_TX5_1:		m_esai.writeTX(_addr - Esai::M_TX0_1, _val);		return;
		case Esai::M_TSMA_1:	m_esai.writeTSMA(_val);								return;
		case Esai::M_TSMB_1:	m_esai.writeTSMB(_val);								return;
		default:
			break;
		}
		if (_addr != 0xffffd5)
		{
//			LOG("Periph write @ " << std::hex << _addr << ": 0x" << HEX(_val));
		}
		m_mem[_addr - XIO_Reserved_High_First] = _val;
	}

	void Peripherals56367::exec()
	{
	}

	void Peripherals56367::reset()
	{
	}

	void Peripherals56367::setSymbols(Disassembler& _disasm) const
	{
		Esai::setSymbols(_disasm, MemArea_Y);
	}

	void Peripherals56367::terminate()
	{
		m_esai.terminate();
	}
}
