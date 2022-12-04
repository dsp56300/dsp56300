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
		const auto diff = delta(clock, m_lastClock) >> 1;
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

		if (!_t.m_tcr)
		{
			if(_t.m_tcsr.test(Timer::M_TOIE))
				m_peripherals.getDSP().injectInterrupt(Vba_TIMER0_Overflow + (_index << 1));

			_t.m_tcsr.set(Timer::M_TOF);

			if(mode(_index) == ModePWM && _t.m_tcsr.test(Timer::M_TRM))
				_t.m_tcr = _t.m_tlr;
		}

		if (_t.m_tcr == _t.m_tcpr)
		{
			if(_t.m_tcsr.test(Timer::M_TCIE))
				m_peripherals.getDSP().injectInterrupt(Vba_TIMER0_Compare + (_index << 1));

			_t.m_tcsr.set(Timer::M_TCF);

			if(mode(_index) != ModePWM && _t.m_tcsr.test(Timer::M_TRM))
				_t.m_tcr = _t.m_tlr;
		}
	}

	void Timers::writeTCSR(int _index, TWord _val)
	{
//		if(_index != 2)
//			LOG("Write Timer " << _index << " TCSR: " << HEX(_val));

		auto& t = m_timers[_index];

		auto pc = m_peripherals.getDSP().getPC().var;

		// If the timer gets enabled, reset the counter register with the load register content
		if (!t.m_tcsr.test(Timer::M_TE) && bittest<TWord, Timer::M_TE>(_val))
		{
			t.m_tcr = t.m_tlr;

		}
		else if (t.m_tcsr.test(Timer::M_TE) && !bittest<TWord, Timer::M_TE>(_val))
		{
			// force clear of overflow and compare flags below
			_val |= (1<<Timer::M_TOF);
			_val |= (1<<Timer::M_TCF);
//			const auto dsr0 = static_cast<Peripherals56362&>(m_peripherals).getDMA().getDSR(0);
//			LOG("Timer " << _index << " disabled, TCR=" << HEX(t.m_tcr) << ", DSR0=" << dsr0 << ", TPCR=" << HEX(t.m_tcpr));
		}

		timerFlagReset<Timer::M_TOF>(t.m_tcsr, _val);
		timerFlagReset<Timer::M_TCF>(t.m_tcsr, _val);

		t.m_tcsr = _val;
	}
}
