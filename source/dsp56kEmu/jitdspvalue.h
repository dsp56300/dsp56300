#pragma once

#include "jitregtracker.h"
#include "jitregtypes.h"

namespace dsp56k
{
	class DspValue
	{
	public:
		enum Type
		{
			Invalid = -1,
			Memory,
			DspReg24,
			DspReg48,
			DspReg56,
			Immediate24,
			Immediate8,
			HostRegTemp
		};

		void set(const JitRegGP& _reg, Type _type);
		void set(const TWord& _value, Type _type);

		explicit DspValue(JitBlock& _block);
		explicit DspValue(JitBlock& _block, TWord _value, Type _type = Immediate24);

	private:
		JitReg64 m_reg;
		RegGP m_gpTemp;
		Type m_type = Invalid;
		TWord m_immediate = 0;
	};
}
