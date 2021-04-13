#pragma once

#include "types.h"

#include "bitfield.h"

namespace dsp56k
{
	class Timers;

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

	private:
		TWord m_tplr = 0;							// Timer Prescaler Load
		TWord m_tpcr = 0;							// Timer Prescaler Count

		std::array<Timer,3> m_timers;
	};
}
