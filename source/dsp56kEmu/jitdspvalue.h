#pragma once

#include <cstdint>

namespace dsp56k
{
	class DspValue
	{
	public:
		enum Origin
		{
			Memory,
			DspReg24,
			DspReg48,
			DspReg56
		};

	private:
		uint32_t m_bitSize = 0;			// 24, 48, 56, ...
		uint32_t m_shiftAmount = 0;		// if the host register has the value stored with a shift being applied
	};
}
