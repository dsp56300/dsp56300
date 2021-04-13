#pragma once

#include "types.h"

#include "bitfield.h"

namespace dsp56k
{
	class Timers;
	class IPeripherals;

	class Timer
	{
		friend class Timers;

	public:
		enum TcsrBits
		{
			M_TE = 0,								// Timer Enable
			M_TOIE = 1,								// Timer Overflow Interrupt Enable
			M_TCIE = 2,								// Timer Compare Interrupt Enable
			M_TC = 0xF0,							// Timer Control Mask (TC0-TC3)
			M_INV = 8,								// Inverter Bit
			M_TRM = 9,								// Timer Restart Mode

			M_DIR = 11,								// Direction Bit
			M_DI = 12,								// Data Input
			M_DO = 13,								// Data Output

			M_PCE = 15,								// Prescaled Clock Enable

			M_TOF = 20,								// Timer Overflow Flag
			M_TCF = 21,								// Timer Compare Flag

			// Timer Control Bits
			M_TC0 = 4,								// Timer Control 0
			M_TC1 = 5,								// Timer Control 1
			M_TC2 = 6,								// Timer Control 2
			M_TC3 = 7,								// Timer Control 3			
		};

	private:
		TWord m_tlr = 0;							// Timer Load Register
		TWord m_tcpr = 0;							// Timer Compare Register
		TWord m_tcr = 0;							// Timer Count Register

		Bitfield<TWord, TcsrBits, 22> m_tcsr;		// Timer Control/Status Register
	};

	class Timers
	{
	public:
		enum Addresses
		{
			// Register Addresses Of TIMER0
			M_TCSR0 = 0xFFFF8F,						// TIMER0 Control/Status Register
			M_TLR0 = 0xFFFF8E,						// TIMER0 Load Reg
			M_TCPR0 = 0xFFFF8D,						// TIMER0 Compare Register
			M_TCR0 = 0xFFFF8C,						// TIMER0 Count Register

			// Register Addresses Of TIMER1
			M_TCSR1 = 0xFFFF8B,						// TIMER1 Control/Status Register
			M_TLR1 = 0xFFFF8A,						// TIMER1 Load Reg
			M_TCPR1 = 0xFFFF89,						// TIMER1 Compare Register
			M_TCR1 = 0xFFFF88,						// TIMER1 Count Register

			// Register Addresses Of TIMER2
			M_TCSR2 = 0xFFFF87,						// TIMER2 Control/Status Register
			M_TLR2 = 0xFFFF86,						// TIMER2 Load Reg
			M_TCPR2 = 0xFFFF85,						// TIMER2 Compare Register
			M_TCR2 = 0xFFFF84,						// TIMER2 Count Register

			M_TPLR = 0xFFFF83,						// TIMER Prescaler Load Register
			M_TPCR = 0xFFFF82,						// TIMER Prescalar Count Register
		};

		// Timer Prescaler Register Bit Flags
		enum PrescalerBits
		{			
			M_PS = 0x600000,						// Prescaler Source Mask
			M_PS0 = 21,
			M_PS1 = 22,
		};

		Timers(IPeripherals& _peripherals) : m_peripherals(_peripherals) {}
		void exec();
		void execTimer(Timer& _t, uint32_t _index) const;

		void writeTCSR(int _index, TWord _val)
		{
//			LOG("Write Timer " << _index << " TCSR: " << HEX(_val));

			auto& t = m_timers[_index];

			// If the timer gets enabled, reset the counter register with the load register content
			if (!t.m_tcsr.test(Timer::M_TE) && bittest<TWord, Timer::M_TE>(_val))
				t.m_tcr = t.m_tlr;

			timerFlagReset<Timer::M_TOF>(t.m_tcsr, _val);
			timerFlagReset<Timer::M_TCF>(t.m_tcsr, _val);

			t.m_tcsr = _val;
		}
		
		void writeTLR(int _index, TWord _val)		{ m_timers[_index].m_tlr = _val;	LOG("Write Timer " << _index << " TLR: " << HEX(_val)); }
		void writeTCPR(int _index, TWord _val)		{ m_timers[_index].m_tcpr = _val;	}//LOG("Write Timer " << _index << " TCPR: " << HEX(_val)); }
		void writeTCR(int _index, TWord _val)		{ m_timers[_index].m_tcr = _val;	LOG("Write Timer " << _index << " TCR: " << HEX(_val)); }

		void writeTPLR(TWord _val)					{ m_tplr = _val;					LOG("Write Timer TPLR " << ": " << HEX(_val)); }
		void writeTPCR(TWord _val)					{ m_tpcr = _val;					LOG("Write Timer TPCR " << ": " << HEX(_val)); }

		TWord readTCSR(int _index)					{ return m_timers[_index].m_tcsr; }
		TWord readTLR(int _index)					{ return m_timers[_index].m_tlr; }
		TWord readTCPR(int _index)					{ return m_timers[_index].m_tcpr; }
		TWord readTCR(int _index)					{ return m_timers[_index].m_tcr; }

		TWord readTPLR()							{ return m_tplr; }
		TWord readTPCR()							{ return m_tpcr; }

	private:
		template<Timer::TcsrBits B> static void timerFlagReset(const Bitfield<unsigned, Timer::TcsrBits, 22>& _tcsr, TWord& _val)
		{
			// This is so WTF. Why not clearing it when writing a 0???

			// The TOF/TCF bit is set to indicate that counter overflow has occurred. This bit is cleared by writing a 1 to the
			// TOF/TCF bit. Writing a 0 to the TOF/TCF bit has no effect.
			if(!_tcsr.test(B))
				return;

			if(bittest<TWord, B>(_val))
				_val &= ~(1<<B);
			else
				_val |= (1<<B);
		}

		IPeripherals& m_peripherals;

		TWord m_tplr = 0;							// Timer Prescaler Load
		TWord m_tpcr = 0;							// Timer Prescaler Count

		TWord m_prescalerClock = 0;
		uint32_t m_lastClock = 0;
		std::array<Timer,3> m_timers;
	};
}
