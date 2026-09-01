#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "types.h"
#include "utils.h"

namespace dsp56k
{
	class IPeripherals;

	/**
	 * Serial Host Interface of the DSP56362 family, DSP side.
	 *
	 * Register and bit layout are from the DSP56362 User Manual Rev. 3, chapter 7. The SHI can be
	 * an SPI or I2C endpoint and a master or a slave; what is implemented here is the SPI slave
	 * case, which is what the devices we emulate use: a peer drives the clock, and every transfer
	 * simultaneously moves one word each way. Call exchange() from the peer for that.
	 *
	 * Master mode is not implemented. If a device ever needs it, that is a transfer initiated by a
	 * write to HTX rather than by the peer, and the clock divider in HCKR starts to matter.
	 */
	class SHI
	{
	public:
		enum Addresses
		{
			HCKR	= 0xFFFF90,		// Clock Control Register
			HCSR	= 0xFFFF91,		// Control/Status Register
			HSAR	= 0xFFFF92,		// I2C Slave Address Register
			HTX		= 0xFFFF93,		// Host Transmit Data Register, write only
			HRX		= 0xFFFF94		// Host Receive Data FIFO, read only
		};

		// HCSR, control bits 13-0
		enum ControlBits
		{
			HCSR_HEN	= 0,		// Host Enable
			HCSR_HI2C	= 1,		// I2C (1) or SPI (0)
			HCSR_HM0	= 2,		// HM[1:0], SHI mode
			HCSR_HM1	= 3,
			HCSR_HCKFR	= 4,		// I2C Clock Freeze
			HCSR_HFIFO	= 5,		// receive FIFO is 10 words (1) or 1 word (0)
			HCSR_HMST	= 6,		// Master mode
			HCSR_HRQE0	= 7,		// HRQE[1:0], host request enable
			HCSR_HRQE1	= 8,
			HCSR_HIDLE	= 9,		// Idle
			HCSR_HBIE	= 10,		// Bus Error Interrupt Enable
			HCSR_HTIE	= 11,		// Transmit Interrupt Enable
			HCSR_HRIE0	= 12,		// HRIE[1:0], receive interrupt enable
			HCSR_HRIE1	= 13
		};

		// HCSR, read only status bits. 16, 18 and 23 are reserved
		enum StatusBits
		{
			HCSR_HTUE	= 14,		// Transmit Underrun Error
			HCSR_HTDE	= 15,		// Transmit Data Empty
			HCSR_HRNE	= 17,		// Receive FIFO Not Empty
			HCSR_HRFF	= 19,		// Receive FIFO Full
			HCSR_HROE	= 20,		// Receive Overrun Error
			HCSR_HBER	= 21,		// Bus Error
			HCSR_HBUSY	= 22		// Host Busy
		};

		static constexpr uint32_t FifoSizeLarge	= 10;	// HFIFO set
		static constexpr uint32_t FifoSizeSmall	= 1;	// HFIFO clear

		// called with the word the DSP wrote to HTX, at the moment a transfer moves it out
		using CallbackTx = std::function<void(TWord)>;

		explicit SHI(IPeripherals& _peripherals);

		void reset();

		TWord read(TWord _addr);
		void write(TWord _addr, TWord _val);

		bool isEnabled() const						{ return bittest(m_hcsr, HCSR_HEN); }

		/**
		 * Peer side of one SPI transfer. The peer clocks a word in and simultaneously takes
		 * whatever the DSP left in HTX. Returns that word, or the previous one if the DSP has not
		 * written a new one, which also raises the transmit underrun flag as the hardware does.
		 */
		TWord exchange(TWord _fromPeer);

		// true if the DSP has put a fresh word in HTX that no transfer has taken yet
		bool hasTxData() const						{ return !bittest(m_hcsr, HCSR_HTDE); }

		void setCallbackTx(CallbackTx&& _cb)		{ m_callbackTx = std::move(_cb); }

		static void setSymbols(class Disassembler& _disasm);

	private:
		uint32_t fifoSize() const					{ return bittest(m_hcsr, HCSR_HFIFO) ? FifoSizeLarge : FifoSizeSmall; }
		void updateStatus();
		void injectReceiveInterrupt();

		IPeripherals& m_periph;

		TWord m_hckr = 0;
		TWord m_hcsr = 0;
		TWord m_hsar = 0;
		TWord m_htx = 0;

		std::array<TWord, FifoSizeLarge> m_rx{};
		uint32_t m_rxCount = 0;

		CallbackTx m_callbackTx;
	};
}
