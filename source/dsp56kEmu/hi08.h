#pragma once

namespace dsp56k
{
	class HI08
	{
	public:
		enum Addresses
		{
			HSR		= 0xFFFFC3,					// Host Status Register (HSR)
			HRX		= 0xFFFFC6					// Host Receive Register (HRX)
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

		TWord read()
		{
			if(m_data.empty())
				return 0;

			return m_data.pop_front();
		}

		TWord  readStatusRegister()
		{
			// Toggle HI8 "Receive Data Full" bit
			dsp56k::bitset<TWord>(m_hsr, HSR_HRDF, m_data.empty() ? 0 : 1);

			return m_hsr;
		}
		
		void writeStatusRegister(TWord _val)
		{
			m_hsr = _val;
		}
		
		void reset() {}

	private:
		TWord m_hsr = 0;
		RingBuffer<uint32_t, 1024, false> m_data;
	};
}
