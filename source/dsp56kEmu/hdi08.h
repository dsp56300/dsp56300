#pragma once

#include <vector>
#include <functional>
#include <atomic>

#include "opcodetypes.h"
#include "types.h"
#include "ringbuffer.h"
#include "utils.h"
#include "logging.h"
#include "dma.h"

namespace dsp56k
{
	class IPeripherals;
	class Disassembler;

	class HDI08
	{
	public:
		explicit HDI08(IPeripherals& _peripheral);

		enum Addresses
		{
			HCR		= 0xFFFFC2,					// Host Control Register (HCR)
			HSR		= 0xFFFFC3,					// Host Status Register (HSR)
			HPCR	= 0xFFFFC4,					// Host Port Control Register (HPCR)
			HBAR	= 0xFFFFC5,					// Host Base Address Register (HBAR)
			HORX	= 0xFFFFC6,					// Host Receive Register (HORX)
			HOTX	= 0xFFFFC7,					// Host Transmit Register (HOTX)
			HDDR	= 0xFFFFC8,					// Host Data Direction Register (HDDR)
			HDR		= 0xFFFFC9					// Host Data Register (HDR)
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
			HCR_HRIE,					// Host Receive Interrupt Enable
			HCR_HTIE,					// Host Transmit Interrupt Enable
			HCR_HCIE,					// Host Command Interrupt Enable
			HCR_HF2,					// HCR Host Flags 2,3 (HF2,HF3) Bits 3-4
			HCR_HF3,
			HCR_HDM0,					// HCR Host DMA Mode Control Bits (HDM0, HDM1, HDM2) Bits 5-7
			HCR_HDM1,
			HCR_HDM2,
		};

		using CallbackTx = std::function<void()>;
		using CallbackRx = std::function<void()>;

		TWord readStatusRegister();

		TWord readControlRegister() const
		{
			return m_hcr;
		}

		TWord readPortControlRegister() const
		{
			return m_hpcr;
		}

		void writeControlRegister(TWord _val);

		void writeStatusRegister(const TWord _val);

		void writePortControlRegister(const TWord _val)
		{
			LOG("Write HDI08 HPCR " << HEX(_val));
			m_hpcr = _val;
		}

		bool hasTX() const
		{
			return !m_dataTX.empty();
		}
		TWord readTX();
		void writeTX(TWord _val);

		void exec();

		TWord readRX(Instruction _inst);

		void writeRX(const std::vector<TWord>& _data)		{ writeRX(_data.data(), _data.size()); }
		void writeRX(const TWord* _data, size_t _count);
		void clearRX();

		const auto& rxData() const { return m_dataRX; }
		const auto& txData() const { return m_dataTX; }

		bool hasRXData() const {return !m_dataRX.empty();}

		void setPendingHostFlags01(uint32_t _pendingHostFlags)
		{
			m_pendingHostFlags01 = static_cast<int32_t>(_pendingHostFlags);
		}

		bool hasPendingHostFlags01() const
		{
			return m_pendingHostFlags01 >= 0;
		}

		void setHostFlags(uint8_t _flag0, uint8_t _flag1);
		void setHostFlagsWithWait(uint8_t _flag0, uint8_t _flag1);
		bool needsToWaitForHostFlags(uint8_t _flag0, uint8_t _flag1) const;
		void waitUntilBufferEmpty() const;

		void reset();

		bool dataRXFull() const;

		void terminate();

		TWord readHDR() const;
		void writeHDR(TWord _val);

		TWord readHDDR() const;
		void writeHDDR(TWord _val);

		void setTransmitDataAlwaysEmpty(bool _alwaysEmpty)
		{
			m_transmitDataAlwaysEmpty = _alwaysEmpty;
		}

		static void setSymbols(Disassembler& _disasm);

		void injectTXInterrupt();

		bool txInterruptEnabled() const
		{
			return dsp56k::bittest<TWord, HCR_HTIE>(m_hcr);
		}

		bool rxInterruptEnabled() const
		{
			return dsp56k::bittest<TWord, HCR_HRIE>(m_hcr);
		}

		void setWriteTxCallback(const CallbackTx& _callback)
		{
			m_callbackTx = _callback;
		}

		void setRXRateLimit(uint32_t _rateLimit)
		{
			m_rxRateLimit = _rateLimit;
		}

		void setReadRxCallback(const CallbackRx& _callback)
		{
			m_callbackRx = _callback;

			if(!m_callbackRx)
				m_callbackRx = [] {};
		}

	private:
		bool dmaTriggerReceive() const;
		bool dmaTriggerTransmit() const;
		bool hasDmaReceiveTrigger() const;

		TWord m_hsr = 0;
		TWord m_hcr = 0;
		TWord m_hpcr = 0;
		RingBuffer<TWord, 8192, true> m_dataRX;
		RingBuffer<TWord, 8192, true> m_dataTX;
		IPeripherals& m_periph;
		std::atomic<uint32_t> m_pendingTXInterrupts;
		uint32_t m_lastRXClock = 0;
		TWord m_hdr = 0;
		TWord m_hddr = 0;
		bool m_transmitDataAlwaysEmpty = true;
		CallbackTx m_callbackTx;
		CallbackRx m_callbackRx = [] {};
		uint32_t m_rxRateLimit;		// minimum number of instructions between two RX interrupts
		bool m_waitServeRXInterrupt = false;
		int32_t m_pendingHostFlags01 = -1;

		DmaChannel::RequestSource m_dmaReqSourceReceive;
		DmaChannel::RequestSource m_dmaReqSourceTransmit;
		Dma* m_dma;
	};
}
