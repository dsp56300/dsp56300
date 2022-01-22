#pragma once

#include <cstdint>

#include "jitregtypes.h"
#include "registers.h"

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
			DspReg56,
			Immediate
		};

		void set(const JitRegGP& _reg, Origin _origin, uint32_t _bitSize, uint32_t _shiftAmount = 0, bool _isMasked = true);

		bool isValid() const
		{
			return m_bitSize > 0;
		}

	private:
		Origin m_origin = Memory;
		EReg m_originReg = Reg_COUNT;
		uint32_t m_bitSize = 0;			// 24, 48, 56, ...
		uint32_t m_shiftAmount = 0;		// if the host register has the value stored with a shift being applied
		bool m_isMasked = false;
		JitReg64 m_reg;
	};
}
