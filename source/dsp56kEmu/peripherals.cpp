#include "pch.h"

#include "peripherals.h"

#include "utils.h"

namespace dsp56k
{
	constexpr uint32_t HSR_HRDF = 0;			// Host Status Register Bit: Receive Data Full

	// _____________________________________________________________________________
	// Peripherals
	//
	PeripheralsDefault::PeripheralsDefault()
		: m_mem(0x0)
	{
		m_mem[XIO_IDR - XIO_Reserved_High_First] = 0x001362;
	}

	void PeripheralsDefault::writeHI8Data(const int32_t* _data, const size_t _count)
	{
		for(size_t i=0; i<_count; ++i)
			m_hi8data.push_back(_data[i] & 0x00ffffff);
	}

	TWord PeripheralsDefault::read(TWord _addr)
	{
//		LOG( "Periph read @ " << std::hex << _addr );

		if(_addr == HostIO_HRX)	// Host Receive Register (HRX)
		{
			if(m_hi8data.empty())
				return 0;

			const auto res = m_hi8data.pop_front();
			write(HostIO_HRX, res);
			return res;
		}


		if (_addr >= XIO_Reserved_High_First)
			_addr -= XIO_Reserved_High_First;

		return m_mem[_addr];
	}

	void PeripheralsDefault::exec()
	{
		// Toggle HI8 "Receive Data Full" bit
		auto hsr = read(HostIO_HSR);
		dsp56k::bitset<TWord>(hsr, HSR_HRDF, m_hi8data.empty() ? 0 : 1);
		write(HostIO_HSR, hsr);
	}
}
