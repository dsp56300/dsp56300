#include "interrupts.h"
#include "peripherals.h"
#include "dsp.h"

#include "timers.h"

namespace dsp56k
{
	void Timers::exec()
	{
		auto& t = m_timers[0];

		execTimer(m_timers[0], 0);
		execTimer(m_timers[1], 1);
		execTimer(m_timers[2], 2);
	}

	void Timers::execTimer(Timer& _t, uint32_t _index) const
	{
		if (!_t.m_tcsr.test(Timer::M_TE))
			return;

		_t.m_tcr++;
		_t.m_tcr &= 0xFFFFFF;

		if (_t.m_tcr == _t.m_tcpr)
		{
			if(_t.m_tcsr.test(Timer::M_TCIE))
				m_peripherals.getDSP().injectInterrupt(Vba_TIMER0_Compare + (_index << 1));

			_t.m_tcsr.set(Timer::M_TCF);
		}
		if (!_t.m_tcr)
		{
			if(_t.m_tcsr.test(Timer::M_TOIE))
				m_peripherals.getDSP().injectInterrupt(Vba_TIMER0_Overflow + (_index << 1));

			_t.m_tcsr.set(Timer::M_TOF);
		}
		if (!_t.m_tcr)
		{
			_t.m_tcr = _t.m_tlr;
		}
	}
}
