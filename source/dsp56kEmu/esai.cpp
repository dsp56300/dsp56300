#include "esai.h"

#include "peripherals.h"

namespace dsp56k
{
	Esai::Esai(IPeripherals& _periph) : m_periph(_periph)
	{
	}	
}
