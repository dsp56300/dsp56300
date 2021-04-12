#pragma once
#include <vector>

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
			HSR_HRDF = 0						// Host Status Register Bit: Receive Data Full
		};
		
		void write(const int32_t* _data, const size_t _count)
		{
			for(size_t i=0; i<_count; ++i)
			{
				m_data.waitNotFull();
				m_data.push_back(_data[i] & 0x00ffffff);
			}
		}
		
		void writeCommandStream(const std::vector<uint32_t>& str)
		{
			for (size_t i=0; i<str.size(); ++i) m_commandStream.push_back(str[i] & 0x00ffffff);
		}

		TWord read()
		{
			if (!m_commandStream.empty())
			{
				const auto ret = m_commandStream.front();
				m_commandStream.erase(m_commandStream.begin());
				return ret;
			}

			if(m_data.empty())
				return 0;

			return m_data.pop_front();
		}

		TWord  readStatusRegister()
		{
			// Toggle HI8 "Receive Data Full" bit
			if (!m_commandStream.empty() || !m_data.empty()) m_hsr |= (1<<HSR_HRDF);
			return m_hsr;
		}
		
		void writeStatusRegister(TWord _val)
		{
			m_hsr = _val;
		}
		
		TWord readControlRegister()
		{
			if (!m_data.empty()) m_hcr=(((m_data[0]>>24) & 3) << 3) | (m_hcr&0xFFFE7);
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
		std::vector<uint32_t> m_commandStream;
		IPeripherals& m_periph;
	};
}
