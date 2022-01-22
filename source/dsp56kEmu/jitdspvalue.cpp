#include "jitdspvalue.h"

#include "jithelper.h"

namespace dsp56k
{
	void DspValue::set(const JitRegGP& _reg, Origin _origin, uint32_t _bitSize, uint32_t _shiftAmount, bool _isMasked)
	{
		m_origin = _origin;
		m_reg = r64(_reg);
		m_bitSize = _bitSize;
		m_isMasked = _isMasked;
		m_shiftAmount = _shiftAmount;
	}	
}
