#pragma once

#include <vector>

#include "types.h"

namespace dsp56k
{
	class Esai;
	class IPeripherals;

	class EsaiClock
	{
	public:
		EsaiClock(IPeripherals& _peripherals) : m_periph(_peripherals) {}
		void exec();

		void setPCTL(TWord _val);
		TWord getPCTL() const { return m_pctl; }

		void setSamplerate(uint32_t _samplerate);
		void setCyclesPerSample(uint32_t _cyclesPerSample);
		void setExternalClockFrequency(uint32_t _freq);

		void setEsaiDivider(Esai* _esai, TWord _clockDivider);

		TWord getRemainingInstructionsForFrameSync(TWord _expectedBitValue) const;
		void onTCCRChanged(Esai* _esai);

	private:
		void updateCyclesPerSample();

		IPeripherals& m_periph;

		uint32_t m_lastClock = 0;
		uint32_t m_cyclesPerSample = 2133;			// estimated cycles per sample before calculated
		uint32_t m_fixedCyclesPerSample = 0;
		uint32_t m_samplerate = 0;
		TWord m_pctl = 0;
		TWord m_cyclesSinceWrite = 0;
		uint32_t m_externalClockFrequency = 12000000;	// Hz

		struct EsaiEntry
		{
			Esai* esai = nullptr;

			uint32_t clockDivider = 0;
			uint32_t clockCounter = 0;
		};

		std::vector<EsaiEntry> m_esais;
		std::vector<Esai*> m_esaisPendingProcess;
	};
}
