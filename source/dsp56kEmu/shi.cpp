#include "shi.h"

#include "disasm.h"
#include "dsp.h"
#include "interrupts.h"
#include "peripherals.h"

namespace dsp56k
{
	SHI::SHI(IPeripherals& _peripherals) : m_periph(_peripherals)
	{
		reset();
	}

	void SHI::reset()
	{
		m_hckr = 0;
		m_hcsr = 0;
		m_hsar = 0;
		m_htx = 0;
		m_rxCount = 0;
		m_rx.fill(0);

		updateStatus();
	}

	void SHI::updateStatus()
	{
		// HTDE is not derived, it is cleared by a write to HTX and set when a transfer takes it,
		// so only the receive side is recomputed here
		if(m_rxCount)
			m_hcsr |= (1 << HCSR_HRNE);
		else
			m_hcsr &= ~static_cast<TWord>(1 << HCSR_HRNE);

		if(m_rxCount >= fifoSize())
			m_hcsr |= (1 << HCSR_HRFF);
		else
			m_hcsr &= ~static_cast<TWord>(1 << HCSR_HRFF);
	}

	void SHI::injectReceiveInterrupt()
	{
		// HRIE[1:0]: 00 disables receive interrupts and the DSP polls HRNE/HRFF instead. 01
		// interrupts on HRNE, 10 on HRFF. The encoding of 11 is in a table we could not extract
		// from the manual, treat it as HRNE, which is the conservative choice - it fires earlier.
		const auto hrie = (m_hcsr >> HCSR_HRIE0) & 3;

		if(!hrie)
			return;

		if(hrie == 2)
		{
			if(bittest(m_hcsr, HCSR_HRFF))
				m_periph.getDSP().injectInterrupt(Vba_SHI_Receive_FIFO_Full);
			return;
		}

		if(bittest(m_hcsr, HCSR_HRNE))
			m_periph.getDSP().injectInterrupt(Vba_SHI_Receive_FIFO_Not_Empty);
	}

	TWord SHI::read(const TWord _addr)
	{
		switch (_addr)
		{
		case HCKR:
			return m_hckr;
		case HCSR:
			return m_hcsr;
		case HSAR:
			return m_hsar;
		case HRX:
			{
				if(!m_rxCount)
					return 0;	// reading an empty FIFO returns undefined data, zero is as good as anything

				const auto res = m_rx[0];

				--m_rxCount;
				for(uint32_t i=0; i<m_rxCount; ++i)
					m_rx[i] = m_rx[i+1];

				// "Reading all data from HRX clears the HRNE flag"
				updateStatus();
				m_hcsr &= ~static_cast<TWord>(1 << HCSR_HROE);

				return res;
			}
		case HTX:
		default:
			return 0;	// HTX is write only
		}
	}

	void SHI::write(const TWord _addr, const TWord _val)
	{
		switch (_addr)
		{
		case HCKR:
			m_hckr = _val;
			return;
		case HSAR:
			m_hsar = _val;
			return;
		case HCSR:
			{
				// the status bits are read only, keep ours and take the control bits from the DSP
				constexpr TWord controlMask = (1 << 14) - 1;

				const auto wasEnabled = bittest(m_hcsr, HCSR_HEN);

				m_hcsr = (m_hcsr & ~controlMask) | (_val & controlMask);

				// clearing HEN puts the SHI in its individual reset state
				if(wasEnabled && !bittest(m_hcsr, HCSR_HEN))
				{
					m_rxCount = 0;
					m_htx = 0;
					m_hcsr &= ~static_cast<TWord>((1 << HCSR_HTUE) | (1 << HCSR_HROE) | (1 << HCSR_HBER));
					m_hcsr |= (1 << HCSR_HTDE);
				}

				updateStatus();

				// enabling the receive interrupt while data is already waiting must not lose it
				injectReceiveInterrupt();
			}
			return;
		case HTX:
			// "Writing to the HTX register by DSP core instructions or by DMA transfers clears the
			// HTDE flag", and doing so retires a pending underrun
			m_htx = _val;
			m_hcsr &= ~static_cast<TWord>((1 << HCSR_HTDE) | (1 << HCSR_HTUE));
			return;
		default:
			return;
		}
	}

	TWord SHI::exchange(const TWord _fromPeer)
	{
		if(!bittest(m_hcsr, HCSR_HEN))
			return 0;

		const auto out = m_htx;

		if(bittest(m_hcsr, HCSR_HTDE))
		{
			// the DSP did not refill HTX in time, the same word goes out again. Only a slave can
			// underrun, which is the mode we implement
			m_hcsr |= (1 << HCSR_HTUE);

			if(bittest(m_hcsr, HCSR_HTIE))
				m_periph.getDSP().injectInterrupt(Vba_SHI_Transmit_Underrun_Error);
		}
		else
		{
			m_hcsr |= (1 << HCSR_HTDE);

			if(m_callbackTx)
				m_callbackTx(out);

			if(bittest(m_hcsr, HCSR_HTIE))
				m_periph.getDSP().injectInterrupt(Vba_SHI_Transmit_Data);
		}

		if(m_rxCount >= fifoSize())
		{
			// "the received word is discarded", the FIFO keeps what it has
			m_hcsr |= (1 << HCSR_HROE);

			if((m_hcsr >> HCSR_HRIE0) & 3)
				m_periph.getDSP().injectInterrupt(Vba_SHI_Receive_Overrun_Error);
		}
		else
		{
			m_rx[m_rxCount++] = _fromPeer;
			updateStatus();
			injectReceiveInterrupt();
		}

		return out;
	}

	void SHI::setSymbols(Disassembler& _disasm)
	{
		_disasm.addSymbol(Disassembler::MemX, HCKR, "M_HCKR");
		_disasm.addSymbol(Disassembler::MemX, HCSR, "M_HCSR");
		_disasm.addSymbol(Disassembler::MemX, HSAR, "M_HSAR");
		_disasm.addSymbol(Disassembler::MemX, HTX,  "M_HTX");
		_disasm.addSymbol(Disassembler::MemX, HRX,  "M_HRX");

		_disasm.addSymbol(Disassembler::MemP, Vba_SHI_Transmit_Data,			"int_shi_transmitData");
		_disasm.addSymbol(Disassembler::MemP, Vba_SHI_Transmit_Underrun_Error,	"int_shi_transmitUnderrun");
		_disasm.addSymbol(Disassembler::MemP, Vba_SHI_Receive_FIFO_Not_Empty,	"int_shi_receiveFifoNotEmpty");
		_disasm.addSymbol(Disassembler::MemP, Vba_SHI_Receive_FIFO_Full,		"int_shi_receiveFifoFull");
		_disasm.addSymbol(Disassembler::MemP, Vba_SHI_Receive_Overrun_Error,	"int_shi_receiveOverrun");
		_disasm.addSymbol(Disassembler::MemP, Vba_SHI_Bus_Error,				"int_shi_busError");
	}
}
