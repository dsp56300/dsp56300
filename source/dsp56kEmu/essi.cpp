#include "essi.h"

#include "audio.h"
#include "memory.h"
#include "interrupts.h"
#include "dsp.h"

namespace dsp56k
{
	void Essi::reset()
	{
		/* A hardware RESET signal or software reset instruction clears the port control register and the port
		direction control register, thus configuring all the ESSI signals as GPIO. The ESSI is in the reset
		state while all ESSI signals are programmed as GPIO; it is active only if at least one of the ESSI
		I/O signals is programmed as an ESSI signal. */
		reset(Essi0);
		reset(Essi1);
	}

	void Essi::exec()
	{
		if(m_pendingRXInterrupts > 0 && bittest(get(Essi0, ESSI0_CRB), CRB_RIE))
		{
			--m_pendingRXInterrupts;
			m_periph.getDSP().injectInterrupt(Vba_ESSI0receivedata);			
		}
	}

	void Essi::toggleStatusRegisterBit(const EssiIndex _essi, const uint32_t _bit, const uint32_t _zeroOrOne)
	{
		bitset(m_statusReg, _bit, _zeroOrOne);
	}

	void Essi::setControlRegisters(const EssiIndex _essi, const TWord _cra, const TWord _crb)
	{
		set(_essi, ESSI0_CRA, _cra);
		set(_essi, ESSI0_CRB, _crb);
	}

	TWord Essi::readRX(size_t _index)
	{
		if(!bittest(get(Essi0, ESSI0_CRB), CRB_RE))
			return 0;

		const auto res = readRXimpl(_index);

		toggleStatusRegisterBit(Essi0, SSISR_RFS, m_frameSyncDSPRead);

		return res;
	}

	TWord Essi::readSR()
	{
		// set Receive Register Full flag if there is input
		toggleStatusRegisterBit(Essi0, SSISR_RDF, m_audioInputs[0].empty() ? 0 : 1);

		// set Transmit Register Empty flag if there is space left in the output
		toggleStatusRegisterBit(Essi0, SSISR_TDE, m_audioOutputs[0].full() ? 0 : 1);

		// update frame sync status
		toggleStatusRegisterBit(Essi0, SSISR_RFS, m_frameSyncDSPStatus);
		incFrameSync(m_frameSyncDSPStatus);

		return m_statusReg;
	}

	void Essi::writeTX(uint32_t _txIndex, TWord _val)
	{
		if(!bittest(get(Essi0, ESSI0_CRB), CRB_TE0 - _txIndex))
			return;

		writeTXimpl(_txIndex, _val);
	}

	void Essi::reset(const EssiIndex _index)
	{
		set(_index, ESSI_PRRC, 0);
		set(_index, ESSI_PCRC, 0);
	}

	void Essi::set(const EssiIndex _index, const EssiRegX _reg, const TWord _value)
	{
		m_periph.write(address(_index, _reg), _value);
	}

	TWord Essi::get(const EssiIndex _index, const EssiRegX _reg) const
	{
		return m_periph.read(address(_index, _reg));
	}
};
