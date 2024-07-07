#include "dsp.h"
#include "interrupts.h"
#include "hdi08.h"

namespace dsp56k
{
	HDI08::HDI08(IPeripherals& _peripheral) : m_periph(_peripheral), m_pendingTXInterrupts(0), m_rxRateLimit(200)
	{
	}

	TWord HDI08::readStatusRegister()
	{
		// Toggle HDI8 "Receive Data Full" bit
		dsp56k::bitset<TWord, HSR_HRDF>(m_hsr, m_dataRX.empty() ? 0 : 1);

		// Apply pending host flags, if applicable
		const auto hf01 = m_pendingHostFlags01;
		m_pendingHostFlags01 = -1;

		if(hf01 >= 0)
		{
			m_hsr &= ~0x18;
			m_hsr |= static_cast<uint32_t>(hf01);
		}

		return m_hsr;
	}

	void HDI08::exec()
	{
		if (!bittest(m_hpcr, HPCR_HEN)) 
			return;

		if (!m_waitServeRXInterrupt && rxInterruptEnabled() && !m_dataRX.empty())
		{
			const auto clock = m_periph.getDSP().getInstructionCounter();

			const auto d = delta(clock, m_lastRXClock);
			if(d >= m_rxRateLimit)
			{
				m_periph.getDSP().injectInterrupt(Vba_Host_Receive_Data_Full);
				m_lastRXClock = clock;
				m_waitServeRXInterrupt = true;
//				LOG("Wait serve interrupt");
			}
		}
		else
		{
			if(m_transmitDataAlwaysEmpty)
			{
				if (txInterruptEnabled() && m_pendingTXInterrupts > 0)
				{
					--m_pendingTXInterrupts;
					dsp56k::bitset<TWord, HSR_HTDE>(m_hsr, 1);
//					LOG("HTDE=1");
					m_periph.getDSP().injectInterrupt(Vba_Host_Transmit_Data_Empty);
				}
			}
			else
			{
				if (m_dataTX.empty())
				{
					const auto hadHTDE = bittest(m_hsr, HSR_HTDE);
					const auto injectInterrupt = txInterruptEnabled() && !hadHTDE;
					dsp56k::bitset<TWord, HSR_HTDE>(m_hsr, 1);
//					if(!hadHTDE)
//						LOG("HTDE=1");
					if (injectInterrupt)
					{
//						LOG("Inject HTDE");
						m_periph.getDSP().injectInterrupt(Vba_Host_Transmit_Data_Empty);
					}
				}
			}
		}
	}

	TWord HDI08::readRX(const Instruction _inst)
	{
		if (m_dataRX.empty())
		{
			LOG("Empty read, PC=" << HEX(m_periph.getDSP().getPC().toWord()) << ", processingMode=" << m_periph.getDSP().getProcessingMode());
			m_waitServeRXInterrupt = false;
			return 0;
		}

		TWord res;

		switch (_inst)
		{
		case Btst_pp:
		case Btst_D:
		case Btst_qq:
		case Btst_ea:
		case Btst_aa:
			res = m_dataRX.front();
//			LOG("HDI08 RX = " << HEX(res) << " (non-pop because op is bit test)");
			break;
		default:
			res = m_dataRX.pop_front();
			m_waitServeRXInterrupt = false;
			m_callbackRx();
//			LOG("HDI08 RX = " << HEX(res) << " (pop)");
			break;
		}

		return res;
	}

	void HDI08::writeRX(const TWord* _data, const size_t _count)
	{
		for (size_t i = 0; i < _count; ++i)
		{
			m_dataRX.waitNotFull();
			const auto d = _data[i] & 0x00ffffff;
//			LOG("Write RX: " << HEX(d));
			m_dataRX.push_back(d);
		}
	}

	void HDI08::clearRX()
	{
		m_dataRX.clear();
	}

	void HDI08::setPendingHostFlags01(uint32_t _pendingHostFlags)
	{
		m_pendingHostFlags01 = static_cast<int32_t>(_pendingHostFlags);
	}

	void HDI08::setHostFlags(const uint8_t _flag0, const uint8_t _flag1)
	{
		m_pendingHostFlags01 = -1;

		dsp56k::bitset<TWord, HSR_HF0>(m_hsr, _flag0);
		dsp56k::bitset<TWord, HSR_HF1>(m_hsr, _flag1);

//		LOG("Write HostFlags, HSR " << HEX(m_hsr));
	}

	void HDI08::setHostFlagsWithWait(const uint8_t _flag0, const uint8_t _flag1)
	{
		const auto hsr = m_hsr;
		const auto target = (_flag0 ? 1:0) | (_flag1 ? 2:0);
		if (((hsr>>3)&3) == target) 
			return;
		waitUntilBufferEmpty();
		setHostFlags(_flag0, _flag1);
	}

	void HDI08::waitUntilBufferEmpty() const
	{
		while (hasRXData())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	bool HDI08::needsToWaitForHostFlags(const uint8_t _flag0, const uint8_t _flag1) const
	{
		const auto target = (_flag0?1:0) | (_flag1?2:0);
		const auto hsr = m_hsr;
		if (((hsr>>3)&3)==target) 
			return false;
		return hasRXData();
	}

	void HDI08::reset()
	{
		m_hcr = 0;
		m_hpcr = 0;
		m_hsr = 0;
		bitset<TWord, HSR_HTDE>(m_hsr, 1);
		m_hddr = 0;
		// m_hdr is not affected by reset
	}

	bool HDI08::dataRXFull() const
	{
		return m_dataRX.full();
	}

	void HDI08::terminate()
	{
		while(!m_dataRX.full())
			m_dataRX.push_back(0);
	}

	TWord HDI08::readHDR() const
	{
//		LOG("Read HDR: " << HEX(m_hdr));
		return m_hdr;
	}

	void HDI08::writeHDR(TWord _val)
	{
		LOG("Write HDR: " << HEX(_val));
		m_hdr = _val;
	}

	TWord HDI08::readHDDR() const
	{
		LOG("Read HDDR: " << HEX(m_hdr));
		return m_hddr;
	}

	void HDI08::writeHDDR(TWord _val)
	{
		LOG("Write HDDR: " << HEX(_val));
		m_hddr = _val;
	}

	void HDI08::setSymbols(Disassembler& _disasm)
	{
		constexpr std::pair<int,const char*> symbols[] =
		{
			// HDI08
			{HCR,	"M_HCR"},
			{HSR,	"M_HSR"},
			{HPCR,	"M_HPCR"},
			{HBAR,	"M_HBAR"},
			{HORX,	"M_HORX"},
			{HOTX,	"M_HOTX"},
			{HDDR,	"M_HDDR"},
			{HDR,	"M_HDR"}
		};

		for (const auto& symbol : symbols)
			_disasm.addSymbol(Disassembler::MemX, symbol.first, symbol.second);

		_disasm.addBitSymbol(Disassembler::MemX, HSR, HSR_HRDF, "HSR_HRDF");
		_disasm.addBitSymbol(Disassembler::MemX, HSR, HSR_HTDE, "HSR_HTDE");
		_disasm.addBitSymbol(Disassembler::MemX, HSR, HSR_HCP , "HSR_HCP");
		_disasm.addBitSymbol(Disassembler::MemX, HSR, HSR_HF0 , "HSR_HF0");
		_disasm.addBitSymbol(Disassembler::MemX, HSR, HSR_HF1 , "HSR_HF1");
		_disasm.addBitSymbol(Disassembler::MemX, HSR, HSR_DMA , "HSR_DMA");

		_disasm.addBitSymbol(Disassembler::MemX, HPCR, HPCR_HEN, "HPCR_HEN");

		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HRIE, "HCR_HRIE");
		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HTIE, "HCR_HTIE");
		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HCIE, "HCR_HCIE");

		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HF2, "HCR_HF2");
		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HF3, "HCR_HF3");

		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HDM0, "HCR_HDM0");
		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HDM1, "HCR_HDM1");
		_disasm.addBitSymbol(Disassembler::MemX, HCR, HCR_HDM2, "HCR_HDM2");

		_disasm.addSymbol(Disassembler::MemP, Vba_Host_Receive_Data_Full, "int_hdi08_receiveDataFull");
		_disasm.addSymbol(Disassembler::MemP, Vba_Host_Command, "int_hdi08_hostCommand");
		_disasm.addSymbol(Disassembler::MemP, Vba_Host_Transmit_Data_Empty, "int_hdi08_transmitDataEmpty");
	}

	void HDI08::injectTXInterrupt()
	{
		++m_pendingTXInterrupts;
	}

	uint32_t HDI08::readTX()
	{
		m_dataTX.waitNotEmpty();
		return m_dataTX.pop_front();
	}

	void HDI08::writeTX(const TWord _val)
	{
		if(!m_transmitDataAlwaysEmpty && !m_dataTX.empty())
		{
			LOG("Write HDI08 HOTX: Discarding " << HEX(m_dataTX.front()) << ", HOTX is full, replacing with " << HEX(_val));
			m_dataTX.front() = _val;
			return;
		}

		m_dataTX.waitNotFull();
		m_dataTX.push_back(_val);

//		LOG("Write HDI08 HOTX " << HEX(_val));
//		LOG("HTDE=0");
		dsp56k::bitset<TWord, HSR_HTDE>(m_hsr, 0);

		if(m_transmitDataAlwaysEmpty)
			injectTXInterrupt();

		if(m_callbackTx)
			m_callbackTx();
	}

	void HDI08::writeControlRegister(TWord _val)
	{
//		LOG("Write HDI08 HCR " << HEX(_val) << ", pc=" << HEX(m_periph.getDSP().getPC().toWord()));

		const auto hadTXInterrupt = txInterruptEnabled();
		const auto hadRXInterrupt = rxInterruptEnabled();
		m_hcr = _val;
		const auto hasTXInterrupt = txInterruptEnabled();
		const auto hasRXInterrupt = rxInterruptEnabled();

		if(!hadTXInterrupt && hasTXInterrupt && dsp56k::bittest<TWord, HSR_HTDE>(m_hsr))
		{
			if(m_transmitDataAlwaysEmpty)
				++m_pendingTXInterrupts;
			else
				dsp56k::bitset<TWord, HSR_HTDE>(m_hsr, 0);	// force inject
		}

		return;

		if(!hadTXInterrupt && hasTXInterrupt)
		{
			LOG("HTDE interrupt enabled");
		}
		else if(hadTXInterrupt && !hasTXInterrupt)
		{
			LOG("HTDE interrupt disabled");
		}

		if(!hadRXInterrupt && hasRXInterrupt)
		{
			LOG("RX interrupt enabled");
		}
		else if(hadRXInterrupt && !hasRXInterrupt)
		{
			LOG("RX interrupt disabled");
		}
	}
};
