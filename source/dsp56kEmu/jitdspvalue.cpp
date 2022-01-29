#include "jitdspvalue.h"

#include "jithelper.h"

namespace dsp56k
{
	DspValue::DspValue(JitBlock& _block) : m_gpTemp(_block, false)
	{
	}

	DspValue::DspValue(JitBlock& _block, TWord _value, Type _type): DspValue(_block)
	{
		set(_value, _type);
	}

	void DspValue::set(const JitRegGP& _reg, Type _type)
	{
		m_reg = r64(_reg);
		m_type = _type;
	}

	void DspValue::set(const TWord& _value, Type _type)
	{
		m_immediate = _value;
		m_type = _type;
	}
}
