#pragma once

namespace dsp56k
{
	class IPeripherals;
	class HDI08
	{
	public:
		explicit HDI08(IPeripherals& _peripheral) : m_periph(_peripheral), m_pendingRXInterrupts(0) {}

		enum Addresses
		{
			HCR		= 0xFFFFC2,					// Host Control Register (HCR)
			HSR		= 0xFFFFC3,					// Host Status Register (HSR)
			HPCR	= 0xFFFFC4,					// Host Port Control Register (HPCR)
			HORX	= 0xFFFFC6,					// Host Receive Register (HORX)
			HOTX	= 0xFFFFC7					// Host Transmit Register (HOTX)
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
			
		enum HostPortControlRegisterBits
		{
			HPCR_HEN = 6,					// HostEnable
		};
		
		enum HostControlRegisterBits
		{
			HCR_HRIE = 0,					// Host Receive Interrupt Enable
			HCR_HTIE = 1,					// Host Transmit Interrupt Enable
		};
		

		TWord readStatusRegister()
		{
			// Toggle HDI8 "Receive Data Full" bit
			dsp56k::bitset<TWord, HSR_HRDF>(m_hsr, m_data.empty() ? 0 : 1);
			return m_hsr;
		}

		TWord readControlRegister()
		{
			return m_hcr;
		}

		TWord readPortControlRegister()
		{
			return m_hpcr;
		}

		void writeControlRegister(TWord _val);

		void writeStatusRegister(TWord _val)
		{
			LOG("Write HDI08 HSR " << HEX(_val));
			m_hsr = _val;
		}

		void writePortControlRegister(TWord _val)
		{
			LOG("Write HDI08 HPCR " << HEX(_val));
			m_hpcr = _val;
		}

		void writeTX(TWord _val);

		void exec();

		TWord readRX();
		void writeRX(const int32_t* _data, const size_t _count);
		void clearRX();
		
		bool hasDataToSend() {return !m_data.empty();}

		void setHostFlags(const char _flag0, const char _flag1);

		void reset() {}

	private:
		TWord m_hsr = 0;
		TWord m_hcr = 0;
		TWord m_hpcr = 0;
		RingBuffer<uint32_t, 1024, false> m_data;
		IPeripherals& m_periph;
		std::atomic<uint32_t> m_pendingRXInterrupts;
	};
}
