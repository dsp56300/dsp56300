#pragma once

namespace dsp56k
{
	class IPeripherals;
	class HDI08
	{
	public:
		explicit HDI08(IPeripherals& _peripheral) : m_periph(_peripheral)		{}

		enum Addresses
		{
			HSR		= 0xFFFFC3,					// Host Status Register (HSR)
			HCR		= 0xFFFFC4,					// Host Control Register (HCR)
			HRX		= 0xFFFFC6,					// Host Receive Register (HRX)
			HTX	    = 0xFFFFC7					// Host Transmit Register (HTX)
		};

		enum HostStatusRegisterBits
		{
			HSR_HRDF,						// Receive Data Full
			HSR_HTDE,						// Transmit Data Empty
			HSR_HCP,						// Host Command Pending
			HSR_HF0,						// Host Flag 0
			HSR_HF1,						// Host Flag 1
			HSR_DMA = 7,					// DMA Status
		};
		
		void write(const int32_t* _data, const size_t _count)
		{
			for(size_t i=0; i<_count; ++i)
			{
				m_data.waitNotFull();
				m_data.push_back(_data[i] & 0x01ffffff);
			}
		}
		
		TWord read()
		{
			if(m_data.empty())
				return 0;

			return m_data.pop_front() & 0xFFFFFF;
		}

		TWord  readStatusRegister()
		{
			// Toggle HDI8 "Receive Data Full" bit
			dsp56k::bitset<TWord, HSR_HRDF>(m_hsr, m_data.empty() ? 0 : 1);
			return m_hsr;
		}
		
		void writeStatusRegister(TWord _val)
		{
			m_hsr = _val;
		}
		
		TWord readControlRegister()
		{
			return m_hcr;
		}

		void writeControlRegister(TWord _val)
		{
			m_hcr = _val;
		}
		
		void writeTransmitRegister(TWord _val);
		
		void exec();

		void reset() {}

	private:
		TWord m_hsr = 0;
		TWord m_hcr = 0;
		RingBuffer<uint32_t, 1024, false> m_data;
		IPeripherals& m_periph;
	};
}
