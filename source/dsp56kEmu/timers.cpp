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
		const auto clock = *m_dspInstructionCounter;
		auto diff = delta(clock, m_lastClock);

		if(diff < m_timerupdateInterval)
			return;

		diff >>= 1;

		m_lastClock = clock;

//		m_prescalerClock ^= 1;
//		m_tpcr -= m_prescalerClock;

		if(m_tpcr == 0)
			m_tpcr = m_tplr & 0xfffff;

		execTimer(m_timers[0], 0, diff);
		execTimer(m_timers[1], 1, diff);
		execTimer(m_timers[2], 2, diff);
	}

	void Timers::execTimer(Timer& _t, const uint32_t _index, uint32_t _cycles) const
	{
		if (!_t.m_tcsr.test(Timer::M_TE))
			return;

		_t.m_tcr += _cycles;

		if (_t.m_tcr > 0xffffff)
		{
			// Overflow
			_t.m_tcr &= 0xFFFFFF;

			if(_t.m_tcsr.test(Timer::M_TOIE))
				injectInterrupt(Vba_TIMER0_Overflow, _index);
			else
				_t.m_tcsr.set(Timer::M_TOF);

			if(mode(_index) == ModePWM && _t.m_tcsr.test(Timer::M_TRM))
				_t.m_tcr = _t.m_tlr + _t.m_tcr;	// keep the overshoot
		}

		if (_t.m_tcr >= _t.m_tcpr)
		{
			// Compare
			const auto overshoot = _t.m_tcr - _t.m_tcpr;

			if(_t.m_tcsr.test(Timer::M_TCIE))
				injectInterrupt(Vba_TIMER0_Compare, _index);

			_t.m_tcsr.set(Timer::M_TCF);

			if(mode(_index) != ModePWM && _t.m_tcsr.test(Timer::M_TRM))
				_t.m_tcr = _t.m_tlr + overshoot;
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
			// In timer (0-3) and watchdog (9-10) modes, the counter is preloaded with the TLR value after
			// the TE bit is set and the first internal or external clock signal is received.
			const auto m = mode(_index);
			if(m <= 3 || m == ModeWatchdogPulse || m == ModeWatchdogToggle)
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

	void Timers::writeTLR(int _index, TWord _val)
	{
		m_timers[_index].m_tlr = _val;
		LOG("Write Timer " << _index << " TLR: " << HEX(_val));
	}

	void Timers::writeTCPR(int _index, TWord _val)
	{
		m_timers[_index].m_tcpr = _val;
//		LOG("Write Timer " << _index << " TCPR: " << HEX(_val));
	}

	void Timers::writeTCR(int _index, TWord _val)
	{
		m_timers[_index].m_tcr = _val;
		LOG("Write Timer " << _index << " TCR: " << HEX(_val));
	}

	void Timers::writeTPLR(TWord _val)
	{
		m_tplr = _val;
		LOG("Write Timer TPLR " << ": " << HEX(_val));
	}

	void Timers::writeTPCR(TWord _val)
	{
		m_tpcr = _val;
		LOG("Write Timer TPCR " << ": " << HEX(_val));
	}

	void Timers::setDSP(const DSP* _dsp)
	{
		m_dspInstructionCounter = &_dsp->getInstructionCounter();
	}

	void Timers::setTimerUpdateInterval(const TWord _instructions)
	{
		m_timerupdateInterval = _instructions;
	}

	void Timers::setSymbols(Disassembler& _disasm) const
	{
		constexpr std::pair<int,const char*> symbols[] =
		{
			// Timers
			{M_TCSR0, "M_TCSR0"},
			{M_TLR0	, "M_TLR0"},
			{M_TCPR0, "M_TCPR0"},
			{M_TCR0	, "M_TCR0"},
			{M_TCSR1, "M_TCSR1"},
			{M_TLR1	, "M_TLR1"},
			{M_TCPR1, "M_TCPR1"},
			{M_TCR1	, "M_TCR1"},
			{M_TCSR2, "M_TCSR2"},
			{M_TLR2	, "M_TLR2"},
			{M_TCPR2, "M_TCPR2"},
			{M_TCR2	, "M_TCR2"},
			{M_TPLR	, "M_TPLR"},
			{M_TPCR	, "M_TPCR"},
		};

		for (const auto& symbol : symbols)
			_disasm.addSymbol(Disassembler::MemX, symbol.first, symbol.second);

		for(uint32_t i=0; i<m_timers.size(); ++i)
		{
			_disasm.addSymbol(Disassembler::MemP, m_vbaBase + (i<<2)    , "int_timer" + std::to_string(i) + "_compare");
			_disasm.addSymbol(Disassembler::MemP, m_vbaBase + (i<<2) + 2, "int_timer" + std::to_string(i) + "_overflow");
		}
	}

	void Timers::injectInterrupt(const TWord _vba, const uint32_t _index) const
	{
		const auto offset = Vba_TIMER0_Compare - m_vbaBase;
		m_peripherals.getDSP().injectInterrupt(offset + _vba + (_index << 2));
	}
}
