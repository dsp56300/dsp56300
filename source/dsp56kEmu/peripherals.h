#pragma once

#include "esai.h"
#include "essi.h"
#include "hdi08.h"
#include "hi08.h"
#include "timers.h"
#include "types.h"
#include "staticArray.h"

namespace dsp56k
{
	class Disassembler;

	enum XIO
	{
		XIO_Reserved_High_First	= 0xffff80,	// 24 bit mode
		XIO_Reserved_High_Last	= 0xffffff,

		// DMA
		XIO_DCR5				= 0xffffd8,	// DMA 5 Control Register
		XIO_DCO5,							// DMA 5 Counter
		XIO_DDR5,							// DMA 5 Destination Address Register
		XIO_DSR5,							// DMA 5 Source Address Register

		XIO_DCR4,							// DMA 4 Control Register
		XIO_DCO4,							// DMA 4 Counter
		XIO_DDR4,							// DMA 4 Destination Address Register
		XIO_DSR4,							// DMA 4 Source Address Register

		XIO_DCR3,							// DMA 3 Control Register
		XIO_DCO3,							// DMA 3 Counter
		XIO_DDR3,							// DMA 3 Destination Address Register
		XIO_DSR3,							// DMA 3 Source Address Register

		XIO_DCR2,							// DMA 2 Control Register
		XIO_DCO2,							// DMA 2 Counter
		XIO_DDR2,							// DMA 2 Destination Address Register
		XIO_DSR2,							// DMA 2 Source Address Register

		XIO_DCR1,							// DMA 1 Control Register
		XIO_DCO1,							// DMA 1 Counter
		XIO_DDR1,							// DMA 1 Destination Address Register
		XIO_DSR1,							// DMA 1 Source Address Register

		XIO_DCR0,							// DMA 0 Control Register
		XIO_DCO0,							// DMA 0 Counter
		XIO_DDR0,							// DMA 0 Destination Address Register
		XIO_DSR0,							// DMA 0 Source Address Register

		XIO_DOR3,							// DMA Offset Register 3
		XIO_DOR2,							// DMA Offset Register 2 
		XIO_DOR1,							// DMA Offset Register 1
		XIO_DOR0,							// DMA Offset Register 0

		XIO_DSTR,							// DMA Status Register

		XIO_IDR,							// ID Register

		XIO_AAR3,							// Address Attribute Register 3
		XIO_AAR2,							// Address Attribute Register 2
		XIO_AAR1,							// Address Attribute Register 1
		XIO_AAR0,							// Address Attribute Register 0

		XIO_DCR,							// DRAM Control Register
		XIO_BCR,							// Bus Control Register
		XIO_OGDB,							// OnCE GDB Register

		XIO_PCTL,							// PLL Control Register

		XIO_IPRP,							// Interrupt Priority Register Peripheral
		XIO_IPRC							// Interrupt Priority Register Core
	};

	class IPeripherals
	{
	public:
		virtual ~IPeripherals() = default;

		void setDSP(DSP* _dsp)
		{
			m_dsp = _dsp;
		}

		DSP& getDSP()
		{
			return *m_dsp;
		};

		virtual TWord read(TWord _addr) = 0;
		virtual void write(TWord _addr, TWord _value) = 0;
		virtual void exec() = 0;
		virtual void reset() = 0;
		virtual void setSymbols(Disassembler& _disasm) = 0;
		virtual void terminate() = 0;

	private:
		DSP* m_dsp = nullptr;
	};

	class Peripherals56303 : public IPeripherals
	{
		// _____________________________________________________________________________
		// members
		//
		StaticArray<TWord,XIO_Reserved_High_Last - XIO_Reserved_High_First + 1>	m_mem;

		// _____________________________________________________________________________
		// implementation
		//
	public:
		Peripherals56303();
		
		TWord read(TWord _addr) override;
		void write(TWord _addr, TWord _val) override;

		void exec() override;
		void reset() override;

		Essi& getEssi()	{ return m_essi; }
		HI08& getHI08()	{ return m_hi08; }

		void setSymbols(Disassembler& _disasm) override {}

		void terminate() override {};

	private:
		Essi m_essi;
		HI08 m_hi08;
	};

	class Peripherals56362 : public IPeripherals
	{
		// _____________________________________________________________________________
		// members
		//
		StaticArray<TWord,XIO_Reserved_High_Last - XIO_Reserved_High_First + 1>	m_mem;

		// _____________________________________________________________________________
		// implementation
		//
	public:
		Peripherals56362();
		
		TWord read(TWord _addr) override;
		void write(TWord _addr, TWord _val) override;

		void exec() override;
		void reset() override;

		Esai& getEsai()		{ return m_esai; }
		HDI08& getHDI08()	{ return m_hdi08; }
		
		void setSymbols(Disassembler& _disasm) override;

		void terminate() override;

	private:
		Esai m_esai;
		HDI08 m_hdi08;
		Timers m_timers;
		bool m_disableTimers;
	};}
