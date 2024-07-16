#pragma once

#include <cstdint>

#include "registers.h"
#include "types.h"

namespace dsp56k
{
	class DSP;

	class JitDspMode
	{
	public:
		static constexpr uint32_t Uninitialized = 0xffffffff;

		//                                                   SR_CP1 | SR_CP0 | SR_RM | SR_SM | SR_CE | __SR_18 | SR_SA | SR_FV | SR_LF | SR_DM | SR_SC | __SR_12 | SR_S1 | SR_S0 | SR_I1 | SR_I0;
		static constexpr uint32_t SrModeChangeIgnoreBits   = SR_CP1 | SR_CP0                 | SR_CE | __SR_18 | SR_SA | SR_FV | SR_LF | SR_DM |         __SR_12                 | SR_I1 | SR_I0;
		static constexpr uint32_t SrModeChangeRelevantBits = (~SrModeChangeIgnoreBits) & 0xffff00;

		void initialize(const DSP& _dsp);
		auto get() const { return m_mode; }
		AddressingMode getAddressingMode(uint32_t _aguIndex) const;

		bool operator > (const JitDspMode& _m) const		{ return m_mode > _m.m_mode; }
		bool operator < (const JitDspMode& _m) const		{ return m_mode < _m.m_mode; }
		bool operator == (const JitDspMode& _m) const		{ return m_mode == _m.m_mode; }
		bool operator != (const JitDspMode& _m) const		{ return m_mode != _m.m_mode; }

		static AddressingMode calcAddressingMode(const TReg24& _m);

		uint32_t getSR() const;
		uint32_t testSR(SRBit _bit) const;

	private:
		uint32_t m_mode = Uninitialized;
	};
}
