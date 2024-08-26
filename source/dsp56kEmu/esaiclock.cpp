#include "esaiclock.h"

#include <ios>

#include "dsp.h"
#include "logging.h"

#include "peripherals.h"

namespace dsp56k
{
	EsxiClock::EsxiClock(IPeripherals& _peripherals)
		: m_periph(_peripherals)
	{
	}

	void EsxiClock::exec()
	{
		const auto diff = delta(*m_dspInstructionCounter, m_lastClock);

		if(diff < m_cyclesPerSample)
			return;

		m_lastClock += m_cyclesPerSample;

		auto advanceClock = [](Clock& _c)
		{
			if (++_c.counter > static_cast<int32_t>(_c.divider))
			{
				_c.counter = 0;
				return true;
			}
			return false;
		};

		std::array<Esxi*, MaxEsais> processTx;
		uint32_t txCount = 0;
		std::array<Esxi*, MaxEsais> processRx;
		uint32_t rxCount = 0;

		for (auto& e : m_esais)
		{
			if(e.esai->hasEnabledTransmitters() && advanceClock(e.tx))
				processTx[txCount++] = e.esai;

			if (e.esai->hasEnabledReceivers() && advanceClock(e.rx))
				processRx[rxCount++] = e.esai;
		}

		for(size_t i=0; i<txCount; ++i) processTx[i]->execTX();
		for(size_t i=0; i<rxCount; ++i) processRx[i]->execRX();
	}

	void EsxiClock::setPCTL(const TWord _val)
	{
		if(m_pctl == _val)
			return;

		m_pctl = _val;

		updateCyclesPerSample();
	}

	void EsxiClock::setSamplerate(const uint32_t _samplerate)
	{
		if (m_samplerate == _samplerate)
			return;

		m_samplerate = _samplerate;

		updateCyclesPerSample();
	}

	void EsxiClock::setCyclesPerSample(const uint32_t _cyclesPerSample)
	{
		if(m_fixedCyclesPerSample == _cyclesPerSample)
			return;

		m_fixedCyclesPerSample = _cyclesPerSample;

		updateCyclesPerSample();
	}

	void EsxiClock::setExternalClockFrequency(const uint32_t _freq)
	{
		if(_freq == m_externalClockFrequency)
			return;

		m_externalClockFrequency = _freq;
		updateCyclesPerSample();
	}

	void EsxiClock::setDSP(const DSP* _dsp)
	{
		setClockSource(_dsp, ClockSource::Instructions);
	}

	void EsxiClock::setClockSource(const ClockSource _clockSource)
	{
		setClockSource(&m_periph.getDSP(), _clockSource);
	}

	void EsxiClock::restartClock()
	{
		m_lastClock = *m_dspInstructionCounter;
	}

	TWord EsxiClock::getRemainingInstructionsForFrameSync() const
	{
		constexpr TWord offset = 1;

		const auto cyclesSinceWrite = delta(getDspInstructionCounter(), getLastClock());

		if (cyclesSinceWrite >= getCyclesPerSample() - offset)
			return 0;

		const auto periphCycles = getPeripherals().getDSP().getRemainingPeripheralsCycles();

		if(periphCycles < offset)
			return 0;

		const auto diff = getCyclesPerSample() - cyclesSinceWrite - offset;

		return std::min(diff, periphCycles - offset);
	}

	void EsxiClock::setClockSource(const DSP* _dsp, const ClockSource _clockSource)
	{
		switch (_clockSource)
		{
		case ClockSource::Instructions:
			m_dspInstructionCounter = &_dsp->getInstructionCounter();
			break;
		case ClockSource::Cycles:
			m_dspInstructionCounter = &_dsp->getCycles();
			break;
		}
	}

	void EsxiClock::updateCyclesPerSample()
	{
		uint32_t cyclesPerSample = m_fixedCyclesPerSample;

		if(!cyclesPerSample)
		{
			if(!m_pctl)
				return;

			const auto pd = ((m_pctl >> 20) & 15) + 1;
			const auto df = 1 << ((m_pctl >> 12) & 3);
			const auto mf = (m_pctl & 0xfff) + 1;

			m_speedHz = static_cast<uint64_t>(m_externalClockFrequency) * static_cast<uint64_t>(mf) / (static_cast<uint64_t>(pd) * static_cast<uint64_t>(df));

			if (m_samplerate)
				cyclesPerSample = static_cast<uint32_t>((m_speedHz / m_samplerate) >> 1);	// 2 samples = 1 frame (stereo)
			else
				cyclesPerSample = mf * 128 / pd;			// The ratio between external clock and sample period simplifies to this.

			cyclesPerSample *= m_speedPercent;
			cyclesPerSample /= 100;

			// A more full expression would be m_cyclesPerSample = dsp_frequency / samplerate, where
			// dsp_frequency = m_extClock * mf / pd and samplerate = m_extClock/256

			const auto speedMhz = static_cast<double>(m_speedHz) / 1000000.0f * m_speedPercent / 100.0f;
			LOG("Clock speed changed to: " << speedMhz << " Mhz, EXTAL=" << m_externalClockFrequency << " Hz, PCTL=" << HEX(m_pctl) << ", mf=" << HEX(mf) << ", pd=" << HEX(pd) << ", df=" << HEX(df) << " => cycles per sample=" << std::dec << cyclesPerSample << ", predefined samplerate=" << m_samplerate);
		}
		else
		{
			cyclesPerSample *= m_speedPercent;
			cyclesPerSample /= 100;
		}

		m_cyclesPerSample = cyclesPerSample;
		m_lastClock = *m_dspInstructionCounter;
	}
	void EsxiClock::setEsaiDivider(Esxi* _esai, const TWord _dividerTX, const TWord _dividerRX)
	{
		bool found = false;

		for (auto& entry : m_esais)
		{
			if(entry.esai == _esai)
			{
				if(entry.tx.divider == _dividerTX && entry.rx.divider == _dividerRX)
					return;

				entry.tx.divider = _dividerTX;
				entry.rx.divider = _dividerRX;
				found = true;
				break;
			}
		}

		if(!found)
		{
			m_esais.emplace_back(EsaiEntry{ _esai, {_dividerTX}, {_dividerRX} });
		}
	}

	bool EsxiClock::setEsaiCounter(const Esxi* _esai, const int _counterTX, const int _counterRX)
	{
		for (auto& esai : m_esais)
		{
			if(esai.esai == _esai)
			{
				esai.tx.counter = _counterTX;
				esai.rx.counter = _counterRX;
				return true;
			}
		}
		return false;
	}

	bool EsxiClock::setSpeedPercent(const uint32_t _percent)
	{
		if(m_speedPercent == _percent)
			return true;

		if(_percent == 0)
			return false;

		m_speedPercent = _percent;
		updateCyclesPerSample();
		return true;
	}
	
	// ______________________
	//

	EsaiClock::EsaiClock(Peripherals56362& _peripherals) : EsxiClock(_peripherals)
	{
	}

	template<bool ExpectedResult>
	TWord EsaiClock::getRemainingInstructionsForTransmitFrameSync() const
	{
		if (static_cast<bool>(static_cast<Esai*>(getEsais().front().esai)->getTransmitFrameSync()) == ExpectedResult)
		{
			// already reached the desired value
			return 0;
		}

		return getRemainingInstructionsForFrameSync();
	}

	template<bool ExpectedResult>
	TWord EsaiClock::getRemainingInstructionsForReceiveFrameSync() const
	{
		if (static_cast<bool>(static_cast<Esai*>(getEsais().front().esai)->getReceiveFrameSync()) == ExpectedResult)
		{
			// already reached the desired value
			return 0;
		}

		return getRemainingInstructionsForFrameSync();
	}

	EssiClock::EssiClock(Peripherals56303& _peripherals) : EsxiClock(_peripherals)
	{
	}

	template<bool ExpectedResult>
	TWord EssiClock::getRemainingInstructionsForTransmitFrameSync(uint32_t _esaiIndex) const
	{
		if (static_cast<Essi*>(getEsais()[_esaiIndex].esai)->getTransmitFrameSync() == ExpectedResult)
		{
			// already reached the desired value
			return 0;
		}
		return getRemainingInstructionsForFrameSync();
	}

	template<bool ExpectedResult>
	TWord EssiClock::getRemainingInstructionsForReceiveFrameSync(uint32_t _esaiIndex) const
	{
		if (static_cast<Essi*>(getEsais()[_esaiIndex].esai)->getReceiveFrameSync() == ExpectedResult)
		{
			// already reached the desired value
			return 0;
		}
		return getRemainingInstructionsForFrameSync();
	}

	template TWord EssiClock::getRemainingInstructionsForTransmitFrameSync<true>(uint32_t) const;
	template TWord EssiClock::getRemainingInstructionsForTransmitFrameSync<false>(uint32_t) const;
	template TWord EssiClock::getRemainingInstructionsForReceiveFrameSync<true>(uint32_t) const;
	template TWord EssiClock::getRemainingInstructionsForReceiveFrameSync<false>(uint32_t) const;

	template TWord EsaiClock::getRemainingInstructionsForTransmitFrameSync<true>() const;
	template TWord EsaiClock::getRemainingInstructionsForTransmitFrameSync<false>() const;
	template TWord EsaiClock::getRemainingInstructionsForReceiveFrameSync<true>() const;
	template TWord EsaiClock::getRemainingInstructionsForReceiveFrameSync<false>() const;
}
