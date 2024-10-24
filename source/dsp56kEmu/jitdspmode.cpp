#include "jitdspmode.h"

#include "dsp.h"

namespace dsp56k
{
	constexpr uint32_t g_aguShift = 1;

	void JitDspMode::initialize(const DSP& _dsp)
	{
		m_mode = 0;

		for(size_t i=0; i<8; ++i)
		{
			const auto m = calcAddressingMode(_dsp.regs().m[i]);

			m_mode |= static_cast<uint32_t>(m) << (i << g_aguShift);
		}

		// only the upper 16 bits of the SR are used, the 8 LSBs (CCR) are ignored
		auto sr = _dsp.regs().sr.toWord() & 0xffff00;

		// furthermore, ignore SR bits that the code doesn't depend on or evaluates at runtime
		sr &= SrModeChangeRelevantBits;

		m_mode |= sr << 8;
	}

	AddressingMode JitDspMode::getAddressingMode(const uint32_t _aguIndex) const
	{
		return static_cast<AddressingMode>((m_mode >> (_aguIndex << g_aguShift)) & 0x3);
	}

	uint32_t JitDspMode::getSR() const
	{
		return (m_mode >> 8) & 0xffff00;
	}

	uint32_t JitDspMode::testSR(const SRBit _bit) const
	{
		const auto sr = getSR();
		return sr & (1<<_bit);
	}

	AddressingMode JitDspMode::calcAddressingMode(const TReg24& _m)
	{
		const auto m = _m.toWord();

		const auto m16 = m & 0xffff;

		if(m16 == 0xffff)
			return AddressingMode::Linear;

		if(m16 >= 0x8000)
			return AddressingMode::MultiWrapModulo;
		if(!m16)
			return AddressingMode::Bitreverse;
		return AddressingMode::Modulo;
	}
}
