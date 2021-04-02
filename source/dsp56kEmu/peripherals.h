#pragma once

#include "types.h"
#include "staticArray.h"

namespace dsp56k
{
	enum XIO
	{
		XIO_Reserved_High_First	= 0xffff80,
		XIO_Reserved_High_Last	= 0xffffff,

		XIO_Reserved_Low_First	= 0x00ff80,
		XIO_Reserved_Low_Last	= 0x00ffff,

		// DMA
		XIO_DCR5				= 0xffffd8,
		XIO_DCO5,
		XIO_DDR5,
		XIO_DSR5,

		XIO_DCR4,
		XIO_DCO4,
		XIO_DDR4,
		XIO_DSR4,

		XIO_DCR3,
		XIO_DCO3,
		XIO_DDR3,
		XIO_DSR3,

		XIO_DCR2,
		XIO_DCO2,
		XIO_DDR2,
		XIO_DSR2,

		XIO_DCR1,
		XIO_DCO1,
		XIO_DDR1,
		XIO_DSR1,

		XIO_DCR0,
		XIO_DCO0,
		XIO_DDR0,
		XIO_DSR0,

		XIO_DOR3,
		XIO_DOR2,
		XIO_DOR1,
		XIO_DOR0,

		XIO_DSTR,

		XIO_IDR,

		XIO_AAR3,
		XIO_AAR2,
		XIO_AAR1,
		XIO_AAR0,

		XIO_DCR,
		XIO_BCR,
		XIO_OGDB,

		XIO_PCTL,

		XIO_IPRP,
		XIO_IPRC
	};

	class IPeripherals
	{
	public:
		virtual bool isValidAddress( TWord _addr ) const = 0;
		virtual TWord read(TWord _addr) = 0;
		virtual void write(TWord _addr, TWord _value) = 0;
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
		virtual ~PeripheralsDefault() = default;

		bool isValidAddress( TWord _addr ) const override
		{
			if( _addr >= XIO_Reserved_High_First && _addr <= XIO_Reserved_High_Last )	return true;
			if( _addr >= XIO_Reserved_Low_First && _addr <= XIO_Reserved_Low_Last )		return true;
			return false;
		}

		TWord read( TWord _addr ) override
		{
//			LOG( "Periph read @ " << std::hex << _addr );

			if( _addr >= XIO_Reserved_High_First )
				_addr -= XIO_Reserved_High_First;
			else if( _addr >= XIO_Reserved_Low_First )
				_addr -= XIO_Reserved_Low_First;

			return m_mem[_addr];
		}

		void write( TWord _addr, TWord _val )
		{
//			LOG( "Periph write @ " << std::hex << _addr );

			if( _addr >= XIO_Reserved_High_First )
				_addr -= XIO_Reserved_High_First;
			else if( _addr >= XIO_Reserved_Low_First )
				_addr -= XIO_Reserved_Low_First;

			m_mem[_addr] = _val;
		}
	};
}
