#include "pch.h"

#include "essi.h"

#include "memory.h"

namespace dsp56k
{
	void Essi::reset()
	{
		/* A hardware RESET signal or software reset instruction clears the port control register and the port
		direction control register, thus configuring all the ESSI signals as GPIO. The ESSI is in the reset
		state while all ESSI signals are programmed as GPIO; it is active only if at least one of the ESSI
		I/O signals is programmed as an ESSI signal. */
		reset(Essi0);
		reset(Essi1);
	}
	
	void Essi::toggleStatusRegisterBit(const EssiIndex _essi, const uint32_t _bit, const uint32_t _zeroOrOne)
	{
		const auto mask = 1 << _bit;
		const auto bit = _zeroOrOne << _bit;

		auto v = get(_essi, ESSI0_SSISR);
		v &= ~mask;
		v |= bit;
		set(_essi, ESSI0_SSISR, v);
	}

	void Essi::setControlRegisters(const EssiIndex _essi, const TWord _cra, const TWord _crb)
	{
		set(_essi, ESSI0_CRA, _cra);
		set(_essi, ESSI0_CRB, _crb);
	}

	TWord Essi::testStatusRegisterBit(EssiIndex _essi0, RegSSISRbits _bit) const
	{
		const auto res = get(_essi0, ESSI0_SSISR);
		return res & (1<<_bit);
	}

	void Essi::reset(EssiIndex _index)
	{
		set(_index, ESSI_PRRC, 0);
		set(_index, ESSI_PCRC, 0);
	}

	void Essi::set(const EssiIndex _index, const EssiRegX _reg, const TWord _value)
	{
		m_memory.set(MemArea_X, address(_index, _reg), _value);
	}

	TWord Essi::get(const EssiIndex _index, EssiRegX _reg) const
	{
		return m_memory.get(MemArea_X, address(_index, _reg));
	}
};
