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
