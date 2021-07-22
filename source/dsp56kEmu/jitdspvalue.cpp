#include "jitdspvalue.h"

namespace dsp56k
{
	void DspValue::set(JitReg& _reg, Origin _origin, uint32_t _bitSize, uint32_t _shiftAmount, bool _isMasked)
	{
		m_origin = _origin;
		m_reg = _reg.r64();
		m_bitSize = _bitSize;
		m_isMasked = _isMasked;
		m_shiftAmount = _shiftAmount;
	}	
}
