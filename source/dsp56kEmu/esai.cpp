#include "esai.h"

#include "dsp.h"
#include "interrupts.h"
#include "peripherals.h"

namespace dsp56k
{
	Esai::Esai(IPeripherals& _periph, const EMemArea _area, Dma* _dma/* = nullptr*/) : m_periph(_periph), m_area(_area), m_vba((_area == MemArea_Y) ? (Vba_ESAI_1_Receive_Data - Vba_ESAI_Receive_Data) : 0), m_dma(_dma)
	{
		m_tx.fill(0);
		m_rx.fill(0);
		m_txSlot.resize(32);
		m_rxSlot.resize(32);
	}

	bool Esai::execTX()
	{
//		LOG(HEX(&m_periph.getDSP()) << " exec ESAI " << g_memAreaNames[m_area]  <<  " ictr " << m_periph.getDSP().getInstructionCounter());

		const auto tem = m_tcr & M_TEM;

		if(tem)
		{
			// note that this transfers the data in TX that has been written to it before
			writeAudioOutput();

			if (0 == m_txSlotCounter)
				m_sr.set(M_TFS);
			else
				m_sr.clear(M_TFS);

			++m_txSlotCounter;

			if (m_txSlotCounter > getTxWordCount())
			{
				for (size_t i = 0; i < m_txSlotCounter; ++i)
					writeTXimpl(m_txSlot[i]);

				m_txSlotCounter = 0;
				++m_txFrameCounter;

				if (m_tcr.test(M_TLIE))
					injectInterrupt(Vba_ESAI_Transmit_Last_Slot);
			}
		}

		if(tem)
		{
			if (m_sr.test(M_TUE))
			{
				if (m_tcr.test(M_TEIE))
					injectInterrupt(Vba_ESAI_Transmit_Data_with_Exception_Status);

				m_sr.clear(M_TUE);
			}
			else if (m_tcr.test(M_TIE))
			{
				injectInterrupt(Vba_ESAI_Transmit_Data);
			}
		}

		return false;
	}

	void Esai::execRX()
	{
		m_periph.getDSP().injectInterrupt([this]
		{
			readAudioInput();

			++m_rxSlotCounter;

			if(m_rxSlotCounter > getRxWordCount())
			{
				m_rxSlotCounter = 0;
				++m_rxFrameCounter;
			}
		});

		if (m_sr.test(M_ROE))
		{
			if (m_rcr.test(M_REIE))
				injectInterrupt(Vba_ESAI_Receive_Data_With_Exception_Status);
			else
				m_sr.clear(M_ROE);
		}

		if (m_rcr.test(M_RIE))
			injectInterrupt(Vba_ESAI_Receive_Data);
	}

	const TWord& Esai::readStatusRegister() const
	{
		return m_sr;
	}

	void Esai::writeReceiveControlRegister(TWord _val)
	{
		LOG("Write ESAI RCR " << HEX(_val));
		m_rcr = _val;
	}

	void Esai::writeTransmitControlRegister(TWord _val)
	{
//		const auto temOld  = getTxWordCount();

		m_sr.clear(M_TUE);
		LOG("Write ESAI TCR " << HEX(_val));
		m_tcr = _val;
/*
		const auto tem  = getTxWordCount();

		if(tem != temOld && temOld)
		{
			if(m_txSlotCounter != 0)
				__debugbreak();
		}
*/	}

	void Esai::writeTransmitClockControlRegister(TWord _val)
	{
		LOG("Write ESAI TCCR " << HEX(_val));

		const auto oldTxWC = getTxWordCount();
		const auto oldRxWC = getRxWordCount();

		m_tccr = _val;

		const auto newTxWC = getTxWordCount();
		const auto newRxWC = getRxWordCount();

		if(oldTxWC != newTxWC)
			m_txSlotCounter = 0;

		if (oldRxWC != newRxWC)
			m_rxSlotCounter = 0;

		if(m_clock)
			m_clock->onTCCRChanged(this);
	}

	void Esai::writeTX(uint32_t _index, TWord _val)
	{
		m_tx[_index] = _val;
//		LOG(HEX(&m_periph.getDSP()) << " ESAI " << g_memAreaNames[m_area] << " write TX " << _index);

//		const auto enabled = outputEnabled(_index);

//		if(enabled && (m_writtenTX & (1<<_index)))
//			LOG(HEX(&m_periph.getDSP()) << " ESAI " << g_memAreaNames[m_area] << " TX " << _index << " written twice");

		m_writtenTX |= (1<<_index);

		if(m_writtenTX == (m_tcr & M_TEM))
		{
//			if (m_hasReadStatus)
				m_sr.clear(M_TUE);
			m_sr.clear(M_TDE);
		}
	}

	TWord Esai::readRX(const uint32_t _index)
	{
		if(!inputEnabled(_index))
			return 0;

		m_readRX &= ~(1<<_index);

		if(!m_readRX)
		{
			m_sr.clear(M_RDF, M_ROE);
		}

		/* Master to Slave clocking test code
		if(m_area == MemArea_Y)// && _index == 1)
		{
			const auto c = (m_txFrameCounter % (768*2));
			if(m_txSlotCounter == 2 && c == 0)
				return 0xedc987;
		}
		*/
		/* return noise for testing purposes
		if(m_area == MemArea_Y && _index == 0 && m_txSlotCounter == 1 && (m_txFrameCounter & 1) == 1)
		{
			int32_t r = ((rand() & 0xff) << 24) | ((rand() & 0xfff) << 12) | (rand() & 0xfff);
			r >>= 2;	// 25% gain
			r >>= 8;

			return r;
		}
		*/
		return m_rx[_index];
	}

	void Esai::terminate()
	{
		while(true)
		{
			if(!m_audioOutputs.empty())
				m_audioOutputs.pop_front();
			else if(!m_audioInputs.full())
				m_audioInputs.push_back({});
			else
				break;
		}
	}

	std::string Esai::getTccrAsString() const
	{
		std::stringstream ss;
		ss << "M_THCKD=" << bittest(m_tccr, M_THCKD) << ' ';
		ss << "M_TFSD=" << bittest(m_tccr, M_TFSD) << ' ';
		ss << "M_TCKD=" << bittest(m_tccr, M_TCKD) << ' ';
		ss << "M_THCKP=" << bittest(m_tccr, M_THCKP) << ' ';
		ss << "M_TFSP=" << bittest(m_tccr, M_TFSP) << ' ';
		ss << "M_TCKP=" << bittest(m_tccr, M_TCKP) << ' ';
		ss << "M_TFP=" << getTxClockDivider() << ' ';
		ss << "M_TDC=" << getTxWordCount() << ' ';
		ss << "M_TPSR=" << bittest(m_tccr, M_TPSR) << ' ';
		ss << "M_TPM=" << getTxClockPrescale();
		return ss.str();
	}

	std::string Esai::getRccrAsString() const
	{
		std::stringstream ss;
		ss << "M_RHCKD=" << bittest(m_rccr, M_RHCKD) << ' ';
		ss << "M_RFSD=" << bittest(m_rccr, M_RFSD) << ' ';
		ss << "M_RCKD=" << bittest(m_rccr, M_RCKD) << ' ';
		ss << "M_RHCKP=" << bittest(m_rccr, M_RHCKP) << ' ';
		ss << "M_RFSP=" << bittest(m_rccr, M_RFSP) << ' ';
		ss << "M_RCKP=" << bittest(m_rccr, M_RCKP) << ' ';
		ss << "M_RFP=" << getRxClockDivider() << ' ';
		ss << "M_RDC=" << getRxWordCount() << ' ';
		ss << "M_RPSR=" << (getRxClockPrescalerRange() ? 8 : 1) << ' ';
		ss << "M_RPM=" << getRxClockPrescale();
		return ss.str();
	}

	std::string Esai::getTcrAsString() const
	{
		std::stringstream ss;
		ss << "M_TLIE=" << (m_tcr.test(M_TLIE) ? 1 : 0) << ' ';
		ss << "M_TIE=" << (m_tcr.test(M_TIE) ? 1 : 0) << ' ';
		ss << "M_TEDIE=" << (m_tcr.test(M_TEDIE) ? 1 : 0) << ' ';
		ss << "M_TEIE=" << (m_tcr.test(M_TEIE) ? 1 : 0) << ' ';
		ss << "M_TPR=" << (m_tcr.test(M_TPR) ? 1 : 0) << ' ';
		ss << "M_PADC=" << (m_tcr.test(M_PADC) ? 1 : 0) << ' ';
		ss << "M_TFSR=" << (m_tcr.test(M_TFSR) ? 1 : 0) << ' ';
		ss << "M_TFSL=" << (m_tcr.test(M_TFSL) ? 1 : 0) << ' ';
		ss << "M_TSWS=" << ((m_tcr & M_TSWS) >> M_TSWS0) << ' ';
		ss << "M_TMOD=" << ((m_tcr & M_TMOD) >> M_TMOD0) << ' ';
		ss << "M_TWA=" << (m_tcr.test(M_TWA) ? 1 : 0) << ' ';
		ss << "M_TSHFD=" << (m_tcr.test(M_TSHFD) ? 1 : 0) << ' ';
		ss << "M_TEM=" << ((m_tcr & M_TEM) >> M_TE0);
		return ss.str();
	}

	std::string Esai::getRcrAsString() const
	{
		std::stringstream ss;
		ss << "M_RLIE=" << (m_rcr.test(M_RLIE) ? 1 : 0) << ' ';
		ss << "M_RIE=" << (m_rcr.test(M_RIE) ? 1 : 0) << ' ';
		ss << "M_REDIE=" << (m_rcr.test(M_REDIE) ? 1 : 0) << ' ';
		ss << "M_REIE=" << (m_rcr.test(M_REIE) ? 1 : 0) << ' ';
		ss << "M_RPR=" << (m_rcr.test(M_RPR) ? 1 : 0) << ' ';
		ss << "M_RFSR=" << (m_rcr.test(M_RFSR) ? 1 : 0) << ' ';
		ss << "M_RFSL=" << (m_rcr.test(M_RFSL) ? 1 : 0) << ' ';
		ss << "M_RSWS=" << ((m_rcr & M_RSWS) >> M_RSWS0) << ' ';
		ss << "M_RMOD=" << ((m_rcr & M_RMOD) >> M_RMOD0) << ' ';
		ss << "M_RWA=" << (m_rcr.test(M_RWA) ? 1 : 0) << ' ';
		ss << "M_RSHFD=" << (m_rcr.test(M_RSHFD) ? 1 : 0) << ' ';
		ss << "M_REM=" << ((m_rcr & M_REM) >> M_RE0);
		return ss.str();
	}

	void Esai::injectInterrupt(const TWord _interrupt) const
	{
		m_periph.getDSP().injectInterrupt(_interrupt + m_vba);
	}

	void Esai::readAudioInput()
	{
		const auto rem = m_rcr & M_REM;

		if(!rem)
			return;

		if(m_readRX)
			m_sr.set(M_ROE);

		if (m_rxSlotCounter == 0)
		{
			readRXimpl(m_rx);

			for (uint32_t i = 1; i <= getRxWordCount(); ++i)
				readRXimpl(m_rxSlot[i]);
		}
		else
		{
			m_rx = m_rxSlot[m_rxSlotCounter];
		}

		m_readRX = rem;
		m_sr.set(M_RDF);

		if(m_dma)
			m_dma->trigger(DmaChannel::RequestSource::EsaiReceiveData);
	}

	void Esai::writeAudioOutput()
	{
		const auto tem = m_tcr & M_TEM;

		if(!tem)
			return;

		m_txSlot[m_txSlotCounter] = m_tx;

//		m_tx.fill(0);

		if((m_writtenTX & tem) != tem)
		{
			LOG("ESAI transmit underrun, written is " << HEX(m_writtenTX) << ", enabled is " << HEX(tem));
			m_sr.set(M_TUE);
		}

		m_sr.set(M_TDE);
		m_writtenTX = 0;

		if(m_dma)
			m_dma->trigger(DmaChannel::RequestSource::EsaiTransmitData);
	}

	void Esai::writeTSMA(const TWord _tsma)
	{
		m_tsma = _tsma;
	}

	void Esai::writeTSMB(const TWord _tsmb)
	{
		m_tsmb = _tsmb;
	}

	void Esai::setSymbols(Disassembler& _disasm, EMemArea _area)
	{
		constexpr std::pair<int,const char*> symbolsX[] =
		{
			// ESAI
			{M_PCRC	, "M_PCRC"},
			{M_PRRC	, "M_PRRC"},
			{M_PDRC	, "M_PDRC"},
			{M_RSMB	, "M_RSMB"},
			{M_RSMA	, "M_RSMA"},
			{M_TSMB	, "M_TSMB"},
			{M_TSMA	, "M_TSMA"},
			{M_RCCR	, "M_RCCR"},
			{M_RCR	, "M_RCR"},
			{M_TCCR	, "M_TCCR"},
			{M_TCR	, "M_TCR"},
			{M_SAICR, "M_SAICR"},
			{M_SAISR, "M_SAISR"},
			{M_RX3	, "M_RX3"},
			{M_RX2	, "M_RX2"},
			{M_RX1	, "M_RX1"},
			{M_RX0	, "M_RX0"},
			{M_TSR	, "M_TSR"},
			{M_TX5	, "M_TX5"},
			{M_TX4	, "M_TX4"},
			{M_TX3	, "M_TX3"},
			{M_TX2	, "M_TX2"},
			{M_TX1	, "M_TX1"},
			{M_TX0	, "M_TX0"}
		};
		constexpr std::pair<int, const char*> symbolsY[] =
		{
			// ESAI_1
			{M_RSMB_1	, "M_RSMB_1"},
			{M_RSMA_1	, "M_RSMA_1"},
			{M_TSMB_1	, "M_TSMB_1"},
			{M_TSMA_1	, "M_TSMA_1"},
			{M_RCCR_1	, "M_RCCR_1"},
			{M_RCR_1	, "M_RCR_1"},
			{M_TCCR_1	, "M_TCCR_1"},
			{M_TCR_1	, "M_TCR_1"},
			{M_SAICR_1	, "M_SAICR_1"},
			{M_SAISR_1	, "M_SAISR_1"},
			{M_RX3_1	, "M_RX3_1"},
			{M_RX2_1	, "M_RX2_1"},
			{M_RX1_1	, "M_RX1_1"},
			{M_RX0_1	, "M_RX0_1"},
			{M_TSR_1	, "M_TSR_1"},
			{M_TX5_1	, "M_TX5_1"},
			{M_TX4_1	, "M_TX4_1"},
			{M_TX3_1	, "M_TX3_1"},
			{M_TX2_1	, "M_TX2_1"},
			{M_TX1_1	, "M_TX1_1"},
			{M_TX0_1	, "M_TX0_1"},
		};

		// ESAI bits
		constexpr std::pair<TWord, const char*> esaiRccrBits[] =
		{
			// RCCR Register bits
			{1<<M_RHCKD, "M_RHCKD"},
			{1<<M_RFSD, "M_RFSD"},
			{1<<M_RCKD, "M_RCKD"},
			{1<<M_RHCKP, "M_RHCKP"},
			{1<<M_RFSP, "M_RFSP"},
			{1<<M_RCKP, "M_RCKP"},
			{M_RFP, "M_RFP"},
			{1<<M_RFP3, "M_RFP3"},
			{1<<M_RFP2, "M_RFP2"},
			{1<<M_RFP1, "M_RFP1"},
			{1<<M_RFP0, "M_RFP0"},
			{M_RDC, "M_RDC"},
			{1<<M_RDC4, "M_RDC4"},
			{1<<M_RDC3, "M_RDC3"},
			{1<<M_RDC2, "M_RDC2"},
			{1<<M_RDC1, "M_RDC1"},
			{1<<M_RDC0, "M_RDC0"},
			{1<<M_RPSR, "M_RPSR"},
			{M_RPM, "M_RPM"},
			{1<<M_RPM7, "M_RPM7"},
			{1<<M_RPM6, "M_RPM6"},
			{1<<M_RPM5, "M_RPM5"},
			{1<<M_RPM4, "M_RPM4"},
			{1<<M_RPM3, "M_RPM3"},
			{1<<M_RPM2, "M_RPM2"},
			{1<<M_RPM1, "M_RPM1"},
			{1<<M_RPM0, "M_RPM0"}
		};
		constexpr std::pair<TWord, const char*> esaiTccrBits[] =
		{
			// TCCR register bits
			{1<<M_THCKD, "M_THCKD"},
			{1<<M_TFSD, "M_TFSD"},
			{1<<M_TCKD, "M_TCKD"},
			{1<<M_THCKP, "M_THCKP"},
			{1<<M_TFSP, "M_TFSP"},
			{1<<M_TCKP, "M_TCKP"},
			{M_TFP, "M_TFP"},
			{1<<M_TFP3, "M_TFP3"},
			{1<<M_TFP2, "M_TFP2"},
			{1<<M_TFP1, "M_TFP1"},
			{1<<M_TFP0, "M_TFP0"},
			{M_TDC, "M_TDC"},
			{1<<M_TDC4, "M_TDC4"},
			{1<<M_TDC3, "M_TDC3"},
			{1<<M_TDC2, "M_TDC2"},
			{1<<M_TDC1, "M_TDC1"},
			{1<<M_TDC0, "M_TDC0"},
			{1<<M_TPSR, "M_TPSR"},
			{M_TPM, "M_TPM"},
			{1<<M_TPM7, "M_TPM7"},
			{1<<M_TPM6, "M_TPM6"},
			{1<<M_TPM5, "M_TPM5"},
			{1<<M_TPM4, "M_TPM4"},
			{1<<M_TPM3, "M_TPM3"},
			{1<<M_TPM2, "M_TPM2"},
			{1<<M_TPM1, "M_TPM1"},
			{1<<M_TPM0, "M_TPM0"}
		};
		constexpr std::pair<TWord, const char*> esaiRcrBits[] =
		{
			// RCR Register bits
			{1<<M_RLIE, "M_RLIE"},
			{1<<M_RIE, "M_RIE"},
			{1<<M_REDIE, "M_REDIE"},
			{1<<M_REIE, "M_REIE"},
			{1<<M_RPR, "M_RPR"},
			{1<<M_RFSR, "M_RFSR"},
			{1<<M_RFSL, "M_RFSL"},
			{M_RSWS, "M_RSWS"},
			{1<<M_RSWS4, "M_RSWS4"},
			{1<<M_RSWS3, "M_RSWS3"},
			{1<<M_RSWS2, "M_RSWS2"},
			{1<<M_RSWS1, "M_RSWS1"},
			{1<<M_RSWS0, "M_RSWS0"},
			{M_RMOD, "M_RMOD"},
			{1<<M_RMOD1, "M_RMOD1"},
			{1<<M_RMOD0, "M_RMOD0"},
			{1<<M_RWA, "M_RWA"},
			{1<<M_RSHFD, "M_RSHFD"},
			{M_REM, "M_REM"},
			{1<<M_RE3, "M_RE3"},
			{1<<M_RE2, "M_RE2"},
			{1<<M_RE1, "M_RE1"},
			{1<<M_RE0, "M_RE0"},
		};
		constexpr std::pair<TWord, const char*> esaiTcrBits[] =
		{
			// TCR Register bits
			{1<<M_TLIE, "M_TLIE"},
			{1<<M_TIE, "M_TIE"},
			{1<<M_TEDIE, "M_TEDIE"},
			{1<<M_TEIE, "M_TEIE"},
			{1<<M_TPR, "M_TPR"},
			{1<<M_PADC, "M_PADC"},
			{1<<M_TFSR, "M_TFSR"},
			{1<<M_TFSL, "M_TFSL"},
			{M_TSWS, "M_TSWS"},
			{1<<M_TSWS4, "M_TSWS4"},
			{1<<M_TSWS3, "M_TSWS3"},
			{1<<M_TSWS2, "M_TSWS2"},
			{1<<M_TSWS1, "M_TSWS1"},
			{1<<M_TSWS0, "M_TSWS0"},
			{M_TMOD, "M_TMOD"},
			{1<<M_TMOD1, "M_TMOD1"},
			{1<<M_TMOD0, "M_TMOD0"},
			{1<<M_TWA, "M_TWA"},
			{1<<M_TSHFD, "M_TSHFD"},
			{M_TEM, "M_TEM"},
			{1<<M_TE5, "M_TE5"},
			{1<<M_TE4, "M_TE4"},
			{1<<M_TE3, "M_TE3"},
			{1<<M_TE2, "M_TE2"},
			{1<<M_TE1, "M_TE1"},
			{1<<M_TE0, "M_TE0"}
		};


		constexpr std::pair<TWord, const char*> esaiSrBits[]
		{
			{1<<M_TODE, "M_TODE"},
			{1<<M_TEDE, "M_TEDE"},
			{1<<M_TDE, "M_TDE"},
			{1<<M_TUE, "M_TUE"},
			{1<<M_TFS, "M_TFS"},
			{1<<M_RODF, "M_RODF"},
			{1<<M_REDF, "M_REDF"},
			{1<<M_RDF, "M_RDF"},
			{1<<M_ROE, "M_ROE"},
			{1<<M_RFS, "M_RFS"},
			{1<<M_IF2, "M_IF2"},
			{1<<M_IF1, "M_IF1"},
			{1<<M_IF0, "M_IF0"}
		};

		if(_area == MemArea_X)
		{
			for (const auto& symbol : symbolsX)
				_disasm.addSymbol(Disassembler::MemX, symbol.first, symbol.second);

			for (const auto& bit : esaiRccrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemX, M_RCCR, bit.first, bit.second);
			for (const auto& bit : esaiTccrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemX, M_TCCR, bit.first, bit.second);
			for (const auto& bit : esaiRcrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemX, M_RCR, bit.first, bit.second);
			for (const auto& bit : esaiTcrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemX, M_TCR, bit.first, bit.second);
			for (const auto& bit : esaiSrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemX, M_SAISR, bit.first, bit.second);
		}
		else
		{
			for (const auto& symbol : symbolsY)
				_disasm.addSymbol(Disassembler::MemY, symbol.first, symbol.second);

			for (const auto& bit : esaiRccrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemY, M_RCCR_1, bit.first, bit.second);
			for (const auto& bit : esaiTccrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemY, M_TCCR_1, bit.first, bit.second);
			for (const auto& bit : esaiRcrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemY, M_RCR_1, bit.first, bit.second);
			for (const auto& bit : esaiTcrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemY, M_TCR_1, bit.first, bit.second);
			for (const auto& bit : esaiSrBits)
				_disasm.addBitMaskSymbol(Disassembler::MemY, M_SAISR_1, bit.first, bit.second);
		}

		auto addIR = [&](TWord _addr, const std::string& _name)
		{
			if(_area == MemArea_Y)
				_addr += (Vba_ESAI_1_Receive_Data - Vba_ESAI_Receive_Data);

			_disasm.addSymbol(Disassembler::MemP, _addr, ((_area == MemArea_Y) ? "int_ESAI1_" : "int_ESAI_") + _name);
		};

		addIR(Vba_ESAI_Receive_Data, "ReceiveData");
		addIR(Vba_ESAI_Receive_Even_Data, "ReceiveEvenData");
		addIR(Vba_ESAI_Receive_Data_With_Exception_Status, "ReceiveDataException");
		addIR(Vba_ESAI_Receive_Last_Slot, "ReceiveLastSlot");
		addIR(Vba_ESAI_Transmit_Data, "TransmitData");
		addIR(Vba_ESAI_Transmit_Even_Data, "TransmitEvenData");
		addIR(Vba_ESAI_Transmit_Data_with_Exception_Status, "TransmitDataException");
		addIR(Vba_ESAI_Transmit_Last_Slot, "TransmitLastSlot");
	}
}
