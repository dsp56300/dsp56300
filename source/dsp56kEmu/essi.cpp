#include "essi.h"

#include "dsp.h"
#include "interrupts.h"

#if 1
#define LOGESSI(S)		LOG("ESSI" << m_index << ' ' << S)
#else
#define LOGESSI(S)
#endif

namespace dsp56k
{
	Essi::Essi(Peripherals56303& _peripheral, const uint32_t _essiIndex)
		: m_periph(_peripheral)
		, m_index(_essiIndex)
	{
		m_tx.fill(0);
		m_rx.fill(0);

		m_sr.set(SSISR_TDE);
		m_sr.set(SSISR_TFS);
	}

	void Essi::reset()
	{
		/* A hardware RESET signal or software reset instruction clears the port control register and the port
		direction control register, thus configuring all the ESSI signals as GPIO. The ESSI is in the reset
		state while all ESSI signals are programmed as GPIO; it is active only if at least one of the ESSI
		I/O signals is programmed as an ESSI signal. */
	}

	void Essi::execTX()
	{
		const auto tem = m_crb.testMask(RegCRBbits::CRB_TE);

		if(!tem)
			return;

		// note that this transfers the data in TX that has been written to it before
		writeSlotToFrame();

		if (0 == m_txSlotCounter)
			m_sr.set(RegSSISRbits::SSISR_TFS);
		else
			m_sr.clear(RegSSISRbits::SSISR_TFS);

		++m_txSlotCounter;

		const auto txWordCount = getTxWordCount();
		if (m_txSlotCounter > txWordCount)
		{
			m_txFrame.resize(txWordCount + 1);
			writeTXimpl(m_txFrame);
			m_txFrame.clear();

			m_txSlotCounter = 0;
			++m_txFrameCounter;

			if (m_crb.test(RegCRBbits::CRB_TLIE))
				injectInterrupt(Vba_ESSI0transmitlastslot);
		}

		if (m_sr.test(RegSSISRbits::SSISR_TUE) && m_crb.test(RegCRBbits::CRB_TEIE))
		{
			injectInterrupt(Vba_ESSI0transmitdatawithexceptionstatus);

			m_sr.clear(RegSSISRbits::SSISR_TUE);
		}
		else if (m_crb.test(RegCRBbits::CRB_TIE))
		{
			injectInterrupt(Vba_ESSI0transmitdata);
		}
	}

	void Essi::execRX()
	{
		const auto rem = m_crb.test(RegCRBbits::CRB_RE);

		if(!rem)
			return;

		readSlotFromFrame();

		if (0 == m_rxSlotCounter)
			m_sr.set(SSISR_RFS);
		else
			m_sr.clear(SSISR_RFS);

		if (m_sr.test(RegSSISRbits::SSISR_ROE) && m_crb.test(RegCRBbits::CRB_REIE))
		{
			injectInterrupt(Vba_ESSI0receivedatawithexceptionstatus);
			m_sr.clear(RegSSISRbits::SSISR_ROE);
		}
		else if (m_crb.test(RegCRBbits::CRB_RIE))
		{
			injectInterrupt(Vba_ESSI0receivedata);
		}

		++m_rxSlotCounter;

		if(m_rxSlotCounter > getRxWordCount())
		{
			m_rxSlotCounter = 0;
			++m_rxFrameCounter;
		}
	}

	void Essi::setDSP(DSP* _dsp)
	{
	}

	void Essi::setSymbols(Disassembler& _disasm) const
	{
		auto addSym = [&_disasm](const EssiRegX _reg, const std::string& _name)
		{
			_disasm.addSymbol(Disassembler::MemX, _reg, _name);
		};

		auto addBit = [&_disasm, this](const EssiRegX _reg, const uint32_t _bit, const std::string& _name)
		{
			TWord addr = _reg;
			if(m_index)
				addr -= EssiAddressOffset;
			_disasm.addBitSymbol(Disassembler::MemX, addr, _bit, _name);
		};

		auto addMask = [&_disasm, this](const EssiRegX _reg, const uint32_t _mask, const std::string& _name)
		{
			TWord addr = _reg;
			if(m_index)
				addr -= EssiAddressOffset;
			_disasm.addBitMaskSymbol(Disassembler::MemX, addr, _mask, _name);
		};
		
		auto addIR = [&](TWord _addr, const std::string& _name)
		{
			if(m_index)
				_addr += EssiAddressOffset;

			_disasm.addSymbol(Disassembler::MemP, _addr, (m_index ? "int_ESSI1_" : "int_ESSI0_") + _name);
		};

		if(m_index == 0)
		{
			addSym(ESSI0_TX0, "M_TX00");
			addSym(ESSI0_TX1, "M_TX01");
			addSym(ESSI0_TX2, "M_TX02");
			addSym(ESSI0_TSR, "M_TSR0");
			addSym(ESSI0_RX, "M_RX0");
			addSym(ESSI0_SSISR, "M_SSISR0");
			addSym(ESSI0_CRB, "M_CRB0");
			addSym(ESSI0_CRA, "M_CRA0");
			addSym(ESSI0_TSMA, "M_TSMA0");
			addSym(ESSI0_TSMB, "M_TSMB0");
			addSym(ESSI0_RSMA, "M_RSMA0");
			addSym(ESSI0_RSMB, "M_RSMB0");

			addSym(ESSI_PDRC, "M_PDRC");
			addSym(ESSI_PRRC, "M_PRRC");
			addSym(ESSI_PCRC, "M_PCRC");
		}
		else if(m_index == 1)
		{
			addSym(ESSI1_TX0, "M_TX10");
			addSym(ESSI1_TX1, "M_TX11");
			addSym(ESSI1_TX2, "M_TX12");
			addSym(ESSI1_TSR, "M_TSR1");
			addSym(ESSI1_RX, "M_RX1");
			addSym(ESSI1_SSISR, "M_SSISR1");
			addSym(ESSI1_CRB, "M_CRB1");
			addSym(ESSI1_CRA, "M_CRA1");
			addSym(ESSI1_TSMA, "M_TSMA1");
			addSym(ESSI1_TSMB, "M_TSMB1");
			addSym(ESSI1_RSMA, "M_RSMA1");
			addSym(ESSI1_RSMB, "M_RSMB1");

			addSym(ESSI_PDRD, "M_PDRD");
			addSym(ESSI_PRRD, "M_PRRD");
			addSym(ESSI_PCRD, "M_PCRD");
		}

		addBit(ESSI0_CRA, RegCRAbits::CRA_PM0, "M_PM0");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM1, "M_PM1");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM2, "M_PM2");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM3, "M_PM3");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM4, "M_PM4");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM5, "M_PM5");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM6, "M_PM6");
		addBit(ESSI0_CRA, RegCRAbits::CRA_PM7, "M_PM7");
		addMask(ESSI0_CRA, RegCRAbits::CRA_PM, "M_PM");

		addBit(ESSI0_CRA, RegCRAbits::CRA_PSR, "M_PSR");
		addBit(ESSI0_CRA, RegCRAbits::CRA_DC0, "M_DC0");
		addBit(ESSI0_CRA, RegCRAbits::CRA_DC1, "M_DC1");
		addBit(ESSI0_CRA, RegCRAbits::CRA_DC2, "M_DC2");
		addBit(ESSI0_CRA, RegCRAbits::CRA_DC3, "M_DC3");
		addBit(ESSI0_CRA, RegCRAbits::CRA_DC4, "M_DC4");
		addMask(ESSI0_CRA, RegCRAbits::CRA_DC, "M_DC");

		addBit(ESSI0_CRA, RegCRAbits::CRA_ALC, "M_ALC");

		addBit(ESSI0_CRA, RegCRAbits::CRA_WL0, "M_WL0");
		addBit(ESSI0_CRA, RegCRAbits::CRA_WL1, "M_WL1");
		addBit(ESSI0_CRA, RegCRAbits::CRA_WL2, "M_WL2");
		addMask(ESSI0_CRA, RegCRAbits::CRA_WL, "M_WL");

		addBit(ESSI0_CRA, RegCRAbits::CRA_SSC1, "M_SCC1");

		addBit(ESSI0_CRB, RegCRBbits::CRB_OF0, "M_OF0");
		addBit(ESSI0_CRB, RegCRBbits::CRB_OF1, "M_OF1");
		addMask(ESSI0_CRB, RegCRBbits::CRB_OF, "M_OF");

		addBit(ESSI0_CRB, RegCRBbits::CRB_SCD0, "M_SCD0");
		addBit(ESSI0_CRB, RegCRBbits::CRB_SCD1, "M_SCD1");
		addBit(ESSI0_CRB, RegCRBbits::CRB_SCD2, "M_SCD2");
		addMask(ESSI0_CRB, RegCRBbits::CRB_SCD, "M_SCD");

		addBit(ESSI0_CRB, RegCRBbits::CRB_SCKD, "M_SCKD");
		addBit(ESSI0_CRB, RegCRBbits::CRB_SHFD, "M_SHFD");

		addBit(ESSI0_CRB, RegCRBbits::CRB_FSL0, "M_FSL0");
		addBit(ESSI0_CRB, RegCRBbits::CRB_FSL1, "M_FSL1");
		addMask(ESSI0_CRB, RegCRBbits::CRB_FSL, "M_FSL");

		addBit(ESSI0_CRB, RegCRBbits::CRB_FSR, "M_FSR");
		addBit(ESSI0_CRB, RegCRBbits::CRB_FSP, "M_FSP");
		addBit(ESSI0_CRB, RegCRBbits::CRB_CKP, "M_CKP");
		addBit(ESSI0_CRB, RegCRBbits::CRB_SYN, "M_SYN");
		addBit(ESSI0_CRB, RegCRBbits::CRB_MOD, "M_MOD");

		addBit(ESSI0_CRB, RegCRBbits::CRB_TE0, "M_SSTE0");
		addBit(ESSI0_CRB, RegCRBbits::CRB_TE1, "M_SSTE1");
		addBit(ESSI0_CRB, RegCRBbits::CRB_TE2, "M_SSTE2");
		addMask(ESSI0_CRB, RegCRBbits::CRB_TE, "M_SSTE");

		addBit(ESSI0_CRB, RegCRBbits::CRB_RE, "M_SSRE");
		addBit(ESSI0_CRB, RegCRBbits::CRB_TIE, "M_SSTIE");
		addBit(ESSI0_CRB, RegCRBbits::CRB_RIE, "M_SSRIE");
		addBit(ESSI0_CRB, RegCRBbits::CRB_TLIE, "M_STLIE");
		addBit(ESSI0_CRB, RegCRBbits::CRB_RLIE, "M_SRLIE");
		addBit(ESSI0_CRB, RegCRBbits::CRB_TEIE, "M_STEIE");
		addBit(ESSI0_CRB, RegCRBbits::CRB_REIE, "M_SREIE");

		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_IF0, "M_IF0");
		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_IF1, "M_IF1");
		addMask(ESSI0_SSISR, RegSSISRbits::SSISR_IF, "M_IF");

		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_TFS, "M_TFS");
		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_RFS, "M_RFS");
		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_TUE, "M_TUE");
		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_ROE, "M_ROE");
		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_TDE, "M_TDE");
		addBit(ESSI0_SSISR, RegSSISRbits::SSISR_RDF, "M_RDF");

		addMask(ESSI0_TSMA, 0xffff, "M_SSTSA");
		addMask(ESSI0_TSMB, 0xffff, "M_SSTSB");
		addMask(ESSI0_RSMA, 0xffff, "M_SSRSA");
		addMask(ESSI0_RSMB, 0xffff, "M_SSRSB");

		addIR(Vba_ESSI0receivedata, "ReceiveData");
		addIR(Vba_ESSI0receivedatawithexceptionstatus, "ReceiveDataException");
		addIR(Vba_ESSI0receivelastslot, "ReceiveLastSlot");
		addIR(Vba_ESSI0transmitdata, "TransmitData");
		addIR(Vba_ESSI0transmitdatawithexceptionstatus, "TransmitDataException");
		addIR(Vba_ESSI0transmitlastslot, "TransmitLastSlot");
	}

	TWord Essi::readTX(uint32_t _index)
	{
		LOGESSI("Read TX, returning " << HEX(m_tx[_index]));
		return m_tx[_index];
	}

	TWord Essi::readTSR()
	{
		LOGESSI("Read TSR, returning " << HEX(m_tsr));
		return m_tsr;
	}

	TWord Essi::readRX()
	{
//		LOGESSI("Read RX, returning " << HEX(m_rx));
		if(!m_crb.test(RegCRBbits::CRB_RE))
			return 0;

		m_readRX = 0;

		if(!m_readRX)
			m_sr.clear(RegSSISRbits::SSISR_RDF, RegSSISRbits::SSISR_ROE);

		return m_rx[0];
	}

	const TWord& Essi::readSR() const
	{
//		LOGESSI("Read SR, returning " << HEX(m_sr));
		return m_sr;
	}

	TWord Essi::readCRA()
	{
		LOGESSI("Read CRA, returning " << HEX(m_cra));
		return m_cra;
	}

	TWord Essi::readCRB()
	{
		LOGESSI("Read CRB, returning " << HEX(m_crb));
		return m_crb;
	}

	TWord Essi::readTSMA()
	{
		LOGESSI("Read TSMA, returning " << HEX(m_tsma));
		return m_tsma;
	}

	TWord Essi::readTSMB()
	{
		LOGESSI("Read TSMB, returning " << HEX(m_tsmb));
		return m_tsmb;
	}

	TWord Essi::readRSMA()
	{
		LOGESSI("Read RSMA, returning " << HEX(m_rsma));
		return m_rsma;
	}

	TWord Essi::readRSMB()
	{
		LOGESSI("Read RSMB, returning " << HEX(m_rsmb));
		return m_rsmb;
	}

	void Essi::writeTX(const uint32_t _index, const TWord _val)
	{
		m_tx[_index] = _val;

		m_writtenTX |= (1<<(RegCRBbits::CRB_TE0 - _index));

		if(m_writtenTX == (m_crb & RegCRBbits::CRB_TE))
		{
//			if (m_hasReadStatus)
				m_sr.clear(RegSSISRbits::SSISR_TUE);
			m_sr.clear(RegSSISRbits::SSISR_TDE);
		}
	}

	void Essi::writeTSR(const TWord _val)
	{
		LOGESSI("Write TSR " << "= " << HEX(_val));
		m_tsr = _val;
	}

	void Essi::writeRX(TWord _val)
	{
		LOGESSI("Write RX " << "= " << HEX(_val));
		m_rx[0] = _val;
	}

	void Essi::writeSR(TWord _val)
	{
		LOGESSI("Write SR " << "= " << HEX(_val));
		m_sr = _val;
	}

	void Essi::writeCRA(TWord _val)
	{
		LOGESSI("Write CRA " << "= " << HEX(_val));
		m_cra = _val;
	}

	void Essi::writeCRB(TWord _val)
	{
		LOGESSI("Write CRB " << "= " << HEX(_val));

		const auto remO = m_crb.test(RegCRBbits::CRB_RE);
		const auto temO = m_crb.testMask(CRB_TE);

		m_crb = _val;

		const auto remN = m_crb.test(RegCRBbits::CRB_RE);
		const auto temN = m_crb.testMask(CRB_TE);

		if(remO != remN || temO != temN)
		{
			m_periph.getEssiClock().restartClock();
			if(temO != temN)
				execTX();
		}
	}

	void Essi::writeTSMA(TWord _val)
	{
		LOGESSI("Write TSMA " << "= " << HEX(_val));
		m_tsma = _val;
	}

	void Essi::writeTSMB(TWord _val)
	{
		LOGESSI("Write TSMB " << "= " << HEX(_val));
		m_tsmb = _val;
	}

	void Essi::writeRSMA(TWord _val)
	{
		LOGESSI("Write RSMA " << "= " << HEX(_val));
		m_rsma = _val;
	}

	void Essi::writeRSMB(TWord _val)
	{
		LOGESSI("Write RSMB " << "= " << HEX(_val));
		m_rsmb = _val;
	}

	std::string Essi::getCraAsString() const
	{
		std::stringstream ss;
		ss << "CRA_SSC1=" << (m_cra.test(CRA_SSC1) ? 1 : 0) << ' ';
		ss << "CRA_WL=" << (m_cra.testMask(CRA_WL) >> CRA_WL0) << ' ';
		ss << "CRA_ALC=" << (m_cra.test(CRA_ALC) ? 1 : 0) << ' ';
		ss << "CRA_DC=" << (m_cra.testMask(CRA_DC) >> CRA_DC0) << ' ';
		ss << "CRA_PM=" << (m_cra.testMask(CRA_PM) >> CRA_PM0) << ' ';
		return ss.str();
	}

	std::string Essi::getCrbAsString() const
	{
		std::stringstream ss;
		ss << "CRB_REIE=" << (m_crb.test(CRB_REIE) ? 1 : 0) << ' ';
		ss << "CRB_TEIE=" << (m_crb.test(CRB_TEIE) ? 1 : 0) << ' ';
		ss << "CRB_RLIE=" << (m_crb.test(CRB_RLIE) ? 1 : 0) << ' ';
		ss << "CRB_TLIE=" << (m_crb.test(CRB_TLIE) ? 1 : 0) << ' ';
		ss << "CRB_RIE=" << (m_crb.test(CRB_RIE) ? 1 : 0) << ' ';
		ss << "CRB_RE=" << (m_crb.test(CRB_RE) ? 1 : 0) << ' ';
		ss << "CRB_TE=" << (m_crb.testMask(CRB_TE) >> CRB_TE2) << ' ';
		ss << "CRB_MOD=" << (m_crb.test(CRB_MOD) ? 1 : 0) << ' ';
		ss << "CRB_SYN=" << (m_crb.test(CRB_SYN) ? 1 : 0) << ' ';
		ss << "CRB_CKP=" << (m_crb.test(CRB_CKP) ? 1 : 0) << ' ';
		ss << "CRB_FSP=" << (m_crb.test(CRB_FSP) ? 1 : 0) << ' ';
		ss << "CRB_FSR=" << (m_crb.test(CRB_FSR) ? 1 : 0) << ' ';
		ss << "CRB_FSL=" << (m_crb.testMask(CRB_FSL) >> CRB_FSL0) << ' ';
		ss << "CRB_SHFD=" << (m_crb.test(CRB_SHFD) ? 1 : 0) << ' ';
		ss << "CRB_SCKD=" << (m_crb.test(CRB_SCKD) ? 1 : 0) << ' ';
		ss << "CRB_SCD=" << (m_crb.testMask(CRB_SCD) >> CRB_SCD0) << ' ';
		ss << "CRB_OF=" << (m_crb.testMask(CRB_OF) >> CRB_OF0) << ' ';
		return ss.str();
	}

	TWord Essi::getRxWordCount() const
	{
		return (m_cra & RegCRAbits::CRA_DC) >> RegCRAbits::CRA_DC0;
	}

	TWord Essi::getTxWordCount() const
	{
		return getRxWordCount();
	}

	void Essi::injectInterrupt(TWord _interrupt) const
	{
		m_periph.getDSP().injectInterrupt(_interrupt + m_index * EssiAddressOffset);
	}

	void Essi::readSlotFromFrame()
	{
		const auto rem = m_crb.test(RegCRBbits::CRB_RE);

		if(!rem)
			return;

		if(m_readRX)
			m_sr.set(RegSSISRbits::SSISR_ROE);

		if (m_rxSlotCounter == 0)
			readRXimpl(m_rxFrame);

		if(m_rxSlotCounter < m_rxFrame.size())
			m_rx = m_rxFrame[m_rxSlotCounter];

		m_readRX = rem;
		m_sr.set(RegSSISRbits::SSISR_RDF);

		dmaTrigger(static_cast<uint32_t>(DmaChannel::RequestSource::Essi0ReceiveData));
	}

	void Essi::writeSlotToFrame()
	{
		const auto tem = m_crb.testMask(RegCRBbits::CRB_TE);

		if(!tem)
			return;

		m_txFrame[m_txSlotCounter] = m_tx;

//		m_tx.fill(0);

		if((m_writtenTX & tem) != tem)
		{
			LOGESSI("Transmit underrun, written is " << HEX(m_writtenTX) << ", enabled is " << HEX(tem));
			m_sr.set(RegSSISRbits::SSISR_TUE);
		}

		m_sr.set(RegSSISRbits::SSISR_TDE);
		m_writtenTX = 0;

		dmaTrigger(static_cast<uint32_t>(DmaChannel::RequestSource::Essi0TransmitData));
	}

	void Essi::dmaTrigger(const uint32_t _trigger) const
	{
		constexpr auto off = static_cast<int32_t>(DmaChannel::RequestSource::Essi1TransmitData) - static_cast<int32_t>(DmaChannel::RequestSource::Essi0TransmitData);

		m_periph.getDMA().trigger(static_cast<DmaChannel::RequestSource>(_trigger + m_index * off));
	}
};
