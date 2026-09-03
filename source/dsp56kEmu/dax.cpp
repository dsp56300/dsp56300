#include "dax.h"

#include "disasm.h"
#include "dsp.h"
#include "interrupts.h"
#include "peripherals.h"

namespace dsp56k
{
	DAX::DAX(IPeripherals& _peripherals) : m_periph(_peripherals)
	{
		reset();
	}

	void DAX::reset()
	{
		// hardware and software reset clear the port registers too, the personal reset does not
		m_pdrd = 0;
		m_prrd = 0;
		m_pcrd = 0;
		m_pinsEnabled = false;

		resetTransmitter();
	}

	void DAX::resetTransmitter()
	{
		// "XCTR is cleared by software reset and hardware reset", and so is XSTR. XNADR is
		// explicitly not affected by any of the DAX reset states, so it keeps whatever it holds
		m_xctr = 0;
		m_xstr = 0;

		m_channelA = 0;
		m_channelB = 0;
		m_channelBNext = false;
		m_framePending = false;

		m_frameInBlock = 0;
		m_lastFrameClock = 0;
	}

	void DAX::updatePinsEnabled()
	{
		// Table 10-6: a pin carries its DAX function only when both its control bit in PCRD and
		// its direction bit in PRRD are set, and the DAX stays in its personal reset until at
		// least one of the two pins does. Selecting only one of the two bits leaves the pin as
		// GPIO, so the control register alone is not enough to release the transmitter
		const auto daxPins = m_pcrd & m_prrd;
		const auto enabled = bittest(daxPins, Port_PD0) || bittest(daxPins, Port_PD1);

		if(m_pinsEnabled == enabled)
			return;

		m_pinsEnabled = enabled;

		if(!m_pinsEnabled)
		{
			resetTransmitter();
			return;
		}

		// "The first subframe to be transmitted, immediately after the DAX is enabled, is the
		// beginning of a block"
		m_frameInBlock = 0;
		m_lastFrameClock = m_periph.getDSP().getInstructionCounter();
	}

	void DAX::setClocks(const uint64_t _coreClockHz, const uint32_t _aciClockHz)
	{
		if(m_coreClockHz == _coreClockHz && m_aciClockHz == _aciClockHz)
			return;

		m_coreClockHz = _coreClockHz;
		m_aciClockHz = _aciClockHz;

		updateCyclesPerFrame();
	}

	uint32_t DAX::getSamplerate() const
	{
		// Table 10-3. XCS 00 takes the DSP core clock and assumes it is 1024 x fs, the other three
		// take the ACI pin at 256, 384 or 512 x fs
		const auto xcs = (m_xctr >> XCTR_XCS0) & 3;

		if(!xcs)
			return static_cast<uint32_t>(m_coreClockHz / 1024);

		static constexpr uint32_t g_aciMultiplier[] = { 0, 256, 384, 512 };

		return m_aciClockHz / g_aciMultiplier[xcs];
	}

	void DAX::updateCyclesPerFrame()
	{
		const auto samplerate = getSamplerate();

		m_cyclesPerFrame = samplerate ? static_cast<uint32_t>(m_coreClockHz / samplerate) : 0;
	}

	void DAX::injectInterrupt() const
	{
		// The three sources have their own vectors and their own enables. XBLK does not interrupt
		// on its own; it redirects the audio data register empty interrupt to a second vector so
		// that a different routine can prepare the next block
		if(bittest(m_xstr, XSTR_XAUR) && bittest(m_xctr, XCTR_XUIE))
		{
			m_periph.getDSP().injectInterrupt(Vba_DAX_Underrun_Error);
			return;
		}

		if(!bittest(m_xstr, XSTR_XADE))
			return;

		if(bittest(m_xstr, XSTR_XBLK) && bittest(m_xctr, XCTR_XBIE))
			m_periph.getDSP().injectInterrupt(Vba_DAX_Block_Transferred);
		else if(bittest(m_xctr, XCTR_XDIE))
			m_periph.getDSP().injectInterrupt(Vba_DAX_Audio_Data_Empty);
	}

	void DAX::transmitFrame()
	{
		// XSB forces the next frame to open a new block, and is cleared as that block starts
		if(bittest(m_xctr, XCTR_XSB))
		{
			m_xctr &= ~static_cast<TWord>(1 << XCTR_XSB);
			m_frameInBlock = 0;
		}

		if(m_framePending)
		{
			if(m_callbackTx)
				m_callbackTx(m_channelA, m_channelB, m_xnadr);

			m_framePending = false;
		}
		else
		{
			// "The XAUR status flag is set when the DAX audio data buffers are empty and the
			// respective audio data upload occurs. When a DAX underrun error occurs, the previous
			// frame data will be retransmitted in both channels."
			m_xstr |= (1 << XSTR_XAUR);

			if(m_callbackTx)
				m_callbackTx(m_channelA, m_channelB, m_xnadr);
		}

		// both flags are set as the frame starts and stay set until two channels are written
		m_xstr |= (1 << XSTR_XADE);

		if(m_frameInBlock + 1 >= FramesPerBlock)
			m_xstr |= (1 << XSTR_XBLK);

		injectInterrupt();

		if(++m_frameInBlock >= FramesPerBlock)
			m_frameInBlock = 0;
	}

	uint32_t DAX::exec() noexcept
	{
		if(!m_pinsEnabled || !m_cyclesPerFrame)
			return IPeripherals::MaxDelayCycles;

		const auto clock = m_periph.getDSP().getInstructionCounter();
		const auto elapsed = clock - m_lastFrameClock;

		if(elapsed < m_cyclesPerFrame)
			return static_cast<uint32_t>(m_cyclesPerFrame - elapsed);

		// A long gap between two calls must not turn into a burst of frames, the transmitter runs
		// at a fixed rate. Advance one frame and carry the remainder into the next period
		m_lastFrameClock += m_cyclesPerFrame;

		if(clock - m_lastFrameClock >= m_cyclesPerFrame)
			m_lastFrameClock = clock;

		transmitFrame();

		return m_cyclesPerFrame;
	}

	TWord DAX::read(const TWord _addr)
	{
		switch (_addr)
		{
		case XCTR:
			return m_xctr;
		case XSTR:
			return m_xstr;
		case PDRD:
			return m_pdrd;
		case PRRD:
			return m_prrd;
		case PCRD:
			return m_pcrd;
		case XNADR:
		case XADRA:
		case XADRB:
		default:
			return 0;	// the data registers are write only
		}
	}

	void DAX::write(const TWord _addr, const TWord _val)
	{
		switch (_addr)
		{
		case XCTR:
			{
				// bits 6-23 are reserved and read as zero
				constexpr TWord controlMask = (1 << 6) - 1;

				m_xctr = _val & controlMask;

				updateCyclesPerFrame();
			}
			return;
		case XNADR:
			m_xnadr = _val;
			return;
		case XADRA:
		case XADRB:
			// "Successive write accesses to this register will store channel A and channel B
			// alternately". Two addresses reach the same register, which is what lets a DMA send
			// the non-audio bits and both channels as three transfers to consecutive addresses
			if(m_channelBNext)
			{
				m_channelB = _val;
				m_framePending = true;

				// "XADE is cleared by writing two channels of audio data to XADR", and the same
				// write clears XBLK. XAUR needs XSTR to have been read while it was set, which the
				// read side cannot see, so it is cleared here as well - a program that never reads
				// XSTR cannot tell the difference
				m_xstr &= ~static_cast<TWord>((1 << XSTR_XADE) | (1 << XSTR_XBLK) | (1 << XSTR_XAUR));
			}
			else
			{
				m_channelA = _val;
			}

			m_channelBNext = !m_channelBNext;
			return;
		case PDRD:
			m_pdrd = _val & 3;
			return;
		case PRRD:
			m_prrd = _val & 3;
			updatePinsEnabled();
			return;
		case PCRD:
			m_pcrd = _val & 3;
			updatePinsEnabled();
			return;
		default:
			return;
		}
	}

	void DAX::setSymbols(Disassembler& _disasm)
	{
		_disasm.addSymbol(Disassembler::MemX, XCTR,  "M_XCTR");
		_disasm.addSymbol(Disassembler::MemX, XNADR, "M_XNADR");
		_disasm.addSymbol(Disassembler::MemX, XADRA, "M_XADRA");
		_disasm.addSymbol(Disassembler::MemX, XADRB, "M_XADRB");
		_disasm.addSymbol(Disassembler::MemX, XSTR,  "M_XSTR");
		_disasm.addSymbol(Disassembler::MemX, PDRD,  "M_PDRD");
		_disasm.addSymbol(Disassembler::MemX, PRRD,  "M_PRRD");
		_disasm.addSymbol(Disassembler::MemX, PCRD,  "M_PCRD");

		_disasm.addSymbol(Disassembler::MemP, Vba_DAX_Underrun_Error,		"int_dax_underrunError");
		_disasm.addSymbol(Disassembler::MemP, Vba_DAX_Block_Transferred,	"int_dax_blockTransferred");
		_disasm.addSymbol(Disassembler::MemP, Vba_DAX_Audio_Data_Empty,		"int_dax_audioDataEmpty");
	}
}
