#include "interrupts.h"
#include "peripherals.h"
#include "dsp.h"

#include "timers.h"

namespace dsp56k
{
	uint32_t Timers::exec() noexcept
	{
		// Prescaler Counter
		// The prescaler counter is a 21-bit counter that is decremented on the rising edge of the prescaler input clock.
		// The counter is enabled when at least one of the three timers is enabled (i.e., one or more of the timer enable
		// (TE) bits are set) and is using the prescaler output as its source (i.e., one or more of the PCE bits are set).

		// If the timer runs on internal clock, the frequency is DSP / 2
		const auto clock = *m_dspInstructionCounter;

		const auto diff = clock - m_lastClock;

		if(diff < m_timerupdateInterval)
			return static_cast<uint32_t>(m_timerupdateInterval - diff);

		const uint32_t diffDiv2 = static_cast<uint32_t>(diff >> 1);

		m_lastClock = clock;

		// The prescaler is a 21 bit counter clocked by the prescaler input, reloaded from TPLR when
		// it reaches zero. Its output is one tick per TPLR+1 input clocks, and that output is what
		// clocks a timer with PCE set - a timer without PCE runs straight off the input instead.
		// Ignoring PCE and clocking everything off the input makes a prescaled timer run TPLR+1
		// times too fast, so its compare flag is set permanently rather than periodically.
		//
		// The counter only runs while at least one timer is both enabled and using its output, as
		// the comment at the top of this function already says. While nothing uses it the counter
		// holds, so an idle stretch cannot bank a remainder that makes the first tick after it early.
		bool prescalerEnabled = false;

		for (const auto& t : m_timers)
			prescalerEnabled |= t.m_tcsr.test(Timer::M_TE) && t.m_tcsr.test(Timer::M_PCE);

		uint32_t prescaled = 0;

		if(prescalerEnabled)
		{
			const uint32_t reload = (m_tplr & 0x1fffff) + 1;
			const uint64_t input = static_cast<uint64_t>(m_prescalerRemainder) + diffDiv2;

			prescaled = static_cast<uint32_t>(input / reload);
			m_prescalerRemainder = static_cast<uint32_t>(input % reload);

			// the counter counts down from the preload value, so what is left of it is the reload
			// minus however much of the current period has elapsed
			m_tpcr = reload - 1 - m_prescalerRemainder;
		}

		for(uint32_t i=0; i<3; ++i)
			execTimer(m_timers[i], i, m_timers[i].m_tcsr.test(Timer::M_PCE) ? prescaled : diffDiv2);

		if(diff > m_timerupdateInterval<<1)
			return 0;
		return static_cast<uint32_t>((m_timerupdateInterval << 1) - diff);
	}

	void Timers::execTimer(Timer& _t, const uint32_t _index, uint32_t _cycles) const
	{
		if (!_t.m_tcsr.test(Timer::M_TE))
			return;

		// a prescaled timer sees no clock at all on most calls, and no clock means no compare - the
		// flag below must not be re-raised just because the counter still stands above TCPR
		if (!_cycles)
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
		// "The TPCR is a 24-bit read-only register that reflects the current value in the prescaler
		// counter", DSP56362 UM p201. A write has no effect - and now that exec() derives the count
		// from the prescaler remainder, storing one here would be overwritten on the next call anyway
		LOG("Write Timer TPCR " << ": " << HEX(_val) << ", ignored, the register is read only");
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
