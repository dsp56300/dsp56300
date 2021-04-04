#pragma once

#include "types.h"
#include "staticArray.h"

namespace dsp56k
{
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

	enum HostIO
	{
		HostIO_HSR		= 0xFFFFC3,			// Host Status Register (HSR)
		HostIO_HRX		= 0xFFFFC6			// Host Receive Register (HRX)
	};

	class IPeripherals
	{
	public:
		virtual ~IPeripherals() = default;

		virtual bool isValidAddress( TWord _addr ) const = 0;
		virtual TWord read(TWord _addr) = 0;
		virtual void write(TWord _addr, TWord _value) = 0;
		virtual void exec() = 0;
	};

	// dummy implementation that just stores writes and returns them in subsequent reads
	class PeripheralsDefault : public IPeripherals
	{
		// _____________________________________________________________________________
		// members
		//
		StaticArray<TWord,XIO_Reserved_High_Last - XIO_Reserved_High_First + 1>	m_mem;

		// _____________________________________________________________________________
		// implementation
		//
	public:
		PeripheralsDefault();

		bool isValidAddress( TWord _addr ) const override
		{
			if( _addr >= XIO_Reserved_High_First && _addr <= XIO_Reserved_High_Last )	return true;
			return false;
		}

		TWord read( TWord _addr ) override
		{
//			LOG( "Periph read @ " << std::hex << _addr );

			if( _addr >= XIO_Reserved_High_First )
				_addr -= XIO_Reserved_High_First;

			return m_mem[_addr];
		}

		void write( TWord _addr, TWord _val )
		{
//			LOG( "Periph write @ " << std::hex << _addr );

			if( _addr >= XIO_Reserved_High_First )
				_addr -= XIO_Reserved_High_First;

			m_mem[_addr] = _val;
		}

		void exec() override {}
	};
}
