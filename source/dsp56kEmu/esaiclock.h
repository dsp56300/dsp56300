#pragma once

#include <vector>

#include "types.h"

namespace dsp56k
{
	class Esai;
	class IPeripherals;
	class DSP;

	class EsaiClock
	{
	public:
		static constexpr uint32_t MaxEsais = 2;

		EsaiClock(IPeripherals& _peripherals);
		void exec();

		void setPCTL(TWord _val);
		TWord getPCTL() const { return m_pctl; }

		void setSamplerate(uint32_t _samplerate);
		void setCyclesPerSample(uint32_t _cyclesPerSample);
		void setExternalClockFrequency(uint32_t _freq);

		void setEsaiDivider(Esai* _esai, TWord _divider)
		{
			setEsaiDivider(_esai, _divider, _divider);
		}
		void setEsaiDivider(Esai* _esai, TWord _dividerTX, TWord _dividerRX);
		bool setEsaiCounter(const Esai* _esai, TWord _counter)
		{
			return setEsaiCounter(_esai, _counter, _counter);
		}
		bool setEsaiCounter(const Esai* _esai, TWord _counterTX, TWord _counterRX);

		TWord getRemainingInstructionsForFrameSync(TWord _expectedBitValue) const;
		void onTCCRChanged(Esai* _esai);

		bool setSpeedPercent(uint32_t _percent = 100);

		auto getSpeedInHz() const			{ return m_speedHz; }
		auto getSpeedPercent() const		{ return m_speedPercent; }

		void setDSP(const DSP* _dsp);

	private:
		void updateCyclesPerSample();

		const uint32_t* m_dspInstructionCounter = nullptr;
		uint32_t m_lastClock = 0;
		uint32_t m_cyclesSinceWrite = 0;
		uint32_t m_cyclesPerSample = 2133;				// estimated cycles per sample before calculated

		IPeripherals& m_periph;

		uint32_t m_fixedCyclesPerSample = 0;
		uint32_t m_samplerate = 0;
		TWord m_pctl = 0;
		uint32_t m_externalClockFrequency = 12000000;	// Hz

		uint64_t m_speedHz = 0;							// DSP clock speed in Hertz
		uint32_t m_speedPercent = 100;					// 100% = regular operation, overclock/underclock otherwise

		struct Clock
		{
			uint32_t divider = 0;
			uint32_t counter = 0;
		};

		struct EsaiEntry
		{
			Esai* esai = nullptr;

			Clock tx;
			Clock rx;
		};

		std::vector<EsaiEntry> m_esais;
	};
}
