#include "interrupts.h"
#include "peripherals.h"
#include "dsp.h"

#include "timers.h"

namespace dsp56k
{
	void Timers::exec()
	{
		// Prescaler Counter
		// The prescaler counter is a 21-bit counter that is decremented on the rising edge of the prescaler input clock.
		// The counter is enabled when at least one of the three timers is enabled (i.e., one or more of the timer enable
		// (TE) bits are set) and is using the prescaler output as its source (i.e., one or more of the PCE bits are set).

		// If the timer runs on internal clock, the frequency is DSP / 2
		const auto clock = m_peripherals.getDSP().getInstructionCounter();
		const auto diff = delta(clock, m_lastClock);
		m_lastClock = clock;

//		m_prescalerClock ^= 1;
//		m_tpcr -= m_prescalerClock;

		if(m_tpcr == 0)
			m_tpcr = m_tplr & 0xfffff;

		for (uint32_t i=0;i<diff;i++)
		{
			execTimer(m_timers[0], 0);
			execTimer(m_timers[1], 1);
			execTimer(m_timers[2], 2);
		}
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
