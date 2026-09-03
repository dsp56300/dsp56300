#pragma once

#include <cstdint>
#include <functional>

#include "types.h"
#include "utils.h"

namespace dsp56k
{
	class IPeripherals;

	/**
	 * Digital Audio Transmitter of the DSP56362, the AES/EBU, CP-340 and IEC958 serial output.
	 *
	 * Register and bit layout are from the DSP56362 User Manual Rev. 3, chapter 10. The DAX is
	 * transmit only: there is no receive path to model, so the peer side is a single callback that
	 * is handed one complete frame as it goes out.
	 *
	 * What is modelled is the programmer visible behaviour - the write only data registers, the
	 * A/B channel alternation, and the three status flags with the exact set and clear rules the
	 * manual gives. The biphase encoder, the preamble generator and the parity generator produce
	 * bits on a pin that nothing in an emulator can observe, so they are not modelled; a frame
	 * reaches the callback as the two audio words and the non-audio bits that accompanied them.
	 */
	class DAX
	{
	public:
		enum Addresses
		{
			XCTR	= 0xFFFFD0,		// Control Register
			XNADR	= 0xFFFFD1,		// Non-Audio Data Register, write only
			XADRA	= 0xFFFFD2,		// Audio Data Register, write only
			XADRB	= 0xFFFFD3,		// the same register at a second address, for DMA
			XSTR	= 0xFFFFD4,		// Status Register, read only

			// Port D is the DAX's own two pins used as GPIO, manual section 10.7
			PDRD	= 0xFFFFD5,		// Port D Data Register
			PRRD	= 0xFFFFD6,		// Port D Direction Register
			PCRD	= 0xFFFFD7		// Port D Control Register
		};

		// PCRD and PRRD, bit 0 is ACI/PD0 and bit 1 is ADO/PD1
		enum PortBits
		{
			Port_PD0	= 0,
			Port_PD1	= 1
		};

		// XCTR
		enum ControlBits
		{
			XCTR_XDIE	= 0,		// Audio Data Register Empty Interrupt Enable
			XCTR_XUIE	= 1,		// Underrun Error Interrupt Enable
			XCTR_XBIE	= 2,		// Block Transferred Interrupt Enable
			XCTR_XCS0	= 3,		// XCS[1:0], clock source and frequency
			XCTR_XCS1	= 4,
			XCTR_XSB	= 5			// Start Block, cleared by the hardware when the block starts
		};

		// XSTR, read only
		enum StatusBits
		{
			XSTR_XADE	= 0,		// Audio Data Register Empty
			XSTR_XAUR	= 1,		// Transmit Underrun Error
			XSTR_XBLK	= 2			// the frame being transmitted is the last one of the block
		};

		// XNADR. Everything else in that register is reserved and reads as zero
		enum NonAudioBits
		{
			XNADR_XVA	= 10,		// channel A validity, user data, channel status
			XNADR_XUA	= 11,
			XNADR_XCA	= 12,
			XNADR_XVB	= 13,		// the same three for channel B
			XNADR_XUB	= 14,
			XNADR_XCB	= 15
		};

		static constexpr uint32_t FramesPerBlock = 192;

		// one complete frame on its way out: both audio channels and the non-audio bits with them
		using CallbackTx = std::function<void(TWord _channelA, TWord _channelB, TWord _nonAudio)>;

		explicit DAX(IPeripherals& _peripherals);

		void reset();

		TWord read(TWord _addr);
		void write(TWord _addr, TWord _val);

		uint32_t exec() noexcept;

		/**
		 * The two clocks the frame rate is derived from: the DSP core clock the emulation counts in,
		 * and the frequency at the ACI pin. XCS in XCTR says which of them clocks the transmitter
		 * and at what multiple of the sample rate, so the frame period follows from those alone -
		 * it is not tied to whatever the audio interface happens to be doing.
		 */
		void setClocks(uint64_t _coreClockHz, uint32_t _aciClockHz);

		uint32_t getCyclesPerFrame() const				{ return m_cyclesPerFrame; }
		uint32_t getSamplerate() const;

		bool isEnabled() const							{ return m_pinsEnabled; }

		void setCallbackTx(CallbackTx&& _cb)			{ m_callbackTx = std::move(_cb); }

		static void setSymbols(class Disassembler& _disasm);

	private:
		void resetTransmitter();
		void transmitFrame();
		void injectInterrupt() const;
		void updatePinsEnabled();
		void updateCyclesPerFrame();

		IPeripherals& m_periph;

		TWord m_xctr = 0;
		TWord m_xstr = 0;
		TWord m_xnadr = 0;

		TWord m_pdrd = 0;
		TWord m_prrd = 0;
		TWord m_pcrd = 0;

		// XADR feeding XADBUFA and XADBUFB. The manual describes a FIFO-like path of a register
		// and two buffers, but only the flags it produces are visible to a program, so what is
		// kept here is the frame being assembled and the frame that is ready to go.
		TWord m_channelA = 0;
		TWord m_channelB = 0;
		bool m_channelBNext = false;	// which half the next write to XADR lands in
		bool m_framePending = false;	// a complete frame is waiting for the next transmit slot

		uint32_t m_frameInBlock = 0;
		uint32_t m_cyclesPerFrame = 0;
		uint64_t m_coreClockHz = 0;
		uint32_t m_aciClockHz = 0;
		uint64_t m_lastFrameClock = 0;

		bool m_pinsEnabled = false;

		CallbackTx m_callbackTx;
	};
}
