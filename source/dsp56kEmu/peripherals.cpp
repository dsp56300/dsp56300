#include "pch.h"

#include "peripherals.h"

namespace dsp56k
{
	// _____________________________________________________________________________
	// Peripherals
	//
	Peripherals::Peripherals()
		: m_mem(0x0)
	{
		m_mem[XIO_IDR - XIO_Reserved_High_First] = 0x001362;
	}
}
