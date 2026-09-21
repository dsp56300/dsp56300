#include "dma.h"

// DSP56300FM.pdf chapter 10 (page 181 ff)

#include "dsp.h"
#include "peripherals.h"
#include "utils.h"

#include <cstring> // memcpy

#include "interrupts.h"

#if 0
#include "dsp56kBase/logging.h"
#define LOGDMA(S) LOG(S)
#else
#define LOGDMA(S) do{}while(0)
#endif

namespace dsp56k
{
	namespace
	{
		using RequestSource = DmaChannel::RequestSource;

		/*
		Whether a channel serves a request that its peripheral raised before the channel was enabled.

		"
		DMA Channel Enable
		Enables the channel operation. Setting DE either triggers a single block DMA transfer
		in the DMA transfer mode that uses DE as a trigger or enables a single-block,
		single-line, or single-word DMA transfer in the transfer modes that use a requesting
		device as a trigger."

		The manual calls a peripheral request "a regular peripheral request in which the peripheral can not generate a
		second request until the first one is served". So a request stays raised until a channel serves it, and a
		channel that is enabled while its peripheral requests serves it right away.

		Firmware on the 56362 relies on that:
		- An ESAI transmitter in network mode keeps TDE from its last active slot. A channel that is armed after that
		  slot has to put its first word into slot 0 of the next frame. Without the pending request it only gets the
		  TDE of slot 0, once slot 0 is loaded already, and every word goes out a slot late.
		- Firmware that arms a transmit channel right after the frame sync puts the word for slot 1 first into its
		  block, and firmware that arms a receive channel right after the receive frame sync stores the first word it
		  gets as the one of slot 0.
		- Firmware reads the receive register right before it arms a receive channel, which is only needed if a
		  pending receive request would be served.

		The 56303 keeps waiting for the next request. The 56303 firmware we run arms its ESSI channels with a request
		pending at boot, so serving it would move those streams by a word, and nothing has been seen that asks for it.
		*/

		bool checkTrigger(Peripherals56303& _p, const RequestSource _src)
		{
			return false;
			switch (_src)
			{
			case RequestSource::Essi0TransmitData:			return _p.getEssi0().getSR().test(Essi::SSISR_TDE);
			case RequestSource::Essi0ReceiveData:			return _p.getEssi0().getSR().test(Essi::SSISR_RDF);
			case RequestSource::Essi1TransmitData:			return _p.getEssi1().getSR().test(Essi::SSISR_TDE);
			case RequestSource::Essi1ReceiveData:			return _p.getEssi1().getSR().test(Essi::SSISR_RDF);
			case RequestSource::Hi08ReceiveDataFull:		return _p.getHI08().readStatusRegister() & (1 << dsp56k::HDI08::HSR_HRDF);
			case RequestSource::Hi08TransmitDataEmpty:		return _p.getHI08().readStatusRegister() & (1 << dsp56k::HDI08::HSR_HTDE);
			default:
				assert("Unsupported request source for 56303");
				return false;
			}
		}

		bool checkTrigger(Peripherals56362& _p, const RequestSource _src)
		{
			switch (_src)
			{
			case RequestSource::EsaiReceiveData:			return _p.getEsai().getSR().test(Esai::M_RDF);
			case RequestSource::EsaiTransmitData:			return _p.getEsai().getSR().test(Esai::M_TDE);
			case RequestSource::HostReceiveData:			return _p.getHDI08().readStatusRegister() & (1 << dsp56k::HDI08::HSR_HRDF);
			case RequestSource::HostTransmitData:			return _p.getHDI08().readStatusRegister() & (1 << dsp56k::HDI08::HSR_HTDE);
			// a 56367 board has a second ESAI, in the Y peripherals, served by this same DMA controller
			case RequestSource::Esai1ReceiveData:
				if(auto* p367 = _p.getPeripherals56367())
					return p367->getEsai().getSR().test(Esai::M_RDF);
				return false;
			case RequestSource::Esai1TransmitData:
				if(auto* p367 = _p.getPeripherals56367())
					return p367->getEsai().getSR().test(Esai::M_TDE);
				return false;
			default:
				assert("Unsupported request source for 56362");
				return false;
			}
		}
	}

	DmaChannel::DmaChannel(Dma& _dma, IPeripherals& _peripherals, const TWord _index): m_index(_index), m_dma(_dma), m_peripherals(_peripherals)
	{
	}

	void DmaChannel::setDSR(const TWord _address)
	{
		const bool firstWrite = !m_dsrWritten;
		m_dsr = _address;
		m_dsrWritten = true;
		LOGDMA("DMA set DSR" << m_index << " = " << HEX(_address));
		if (firstWrite && !m_armed && bitvalue(m_dcr, De))
			arm();
	}

	void DmaChannel::setDDR(const TWord _address)
	{
		const bool firstWrite = !m_ddrWritten;
		m_ddr = _address;
		m_ddrWritten = true;
		LOGDMA("DMA set DDR" << m_index << " = " << HEX(_address));
		if (firstWrite && !m_armed && bitvalue(m_dcr, De))
			arm();
	}

	void DmaChannel::setDCO(const TWord _count)
	{
		if (m_dco == _count)
			return;
		m_dco = _count;
		LOGDMA("DMA set DCO" << m_index << " = " << HEX(_count));
		// If DCO is set after DCR(DE=1)+DSR+DDR but before we armed, complete arming now.
		// If already armed, the working counters were initialized from the old DCO value; we
		// leave them as-is to mirror real hardware (writing DCO mid-transfer doesn't reset
		// the working counters).
		if (!m_armed && bitvalue(m_dcr, De))
			arm();
	}

	void DmaChannel::setDCR(const TWord _controlRegister)
	{
		if(m_dcr == _controlRegister)
			return;

		// Re-configuration: drop any previous trigger registration and clear the armed flag
		// so arm() can re-initialize the working counters with the new DCR settings.
		m_dma.removeTriggerTarget(this);
		m_armed = false;

		m_dcr = _controlRegister;

		LOGDMA("DMA set DCR" << m_index << " = " << HEX(_controlRegister));

		arm();
	}

	void DmaChannel::arm()
	{
		// If we are already armed (registered as a request trigger target with initialized
		// working counters), do nothing. setDCR clears m_armed before calling us, so a
		// re-configuration of DCR will go through the full init path again.
		if (m_armed)
			return;

		if (!bitvalue(m_dcr, De))
			return;

		// Some firmwares (e.g. DSP B) write DCR with DE=1 BEFORE configuring
		// DSR/DDR. In that case we cannot complete arming yet — wait for the missing
		// register to be written. setDSR/setDDR will retry arm() on first write.
		if (!m_dsrWritten || !m_ddrWritten)
			return;

		if(bitvalue(m_dcr, D3d))
		{
			extractDCOHML(m_dcoh, m_dcom, m_dcol);

			m_dcohInit = m_dcoh;
			m_dcomInit = m_dcom;
			m_dcolInit = m_dcol;
		}
		else
		{
			// counter mode B splits DCO into DCOH[23-12] and DCOL[11-0], mode A only uses m_dcom
			m_dcohInit = m_dco >> 12;
			m_dcolInit = m_dco & 0xfff;

			m_dcoh = m_dcohInit;
			m_dcol = m_dcolInit;

			// we use dcoM as backup storage for the full DCO reg for all non-3D transfer modes
			m_dcomInit = m_dco;
			m_dcom = m_dcomInit;
		}

		if (!isRequestTrigger())
		{
			m_armed = true;

			m_dma.setActiveChannel(m_index);

			// "When the needed resources are available, each word transfer performed by the DMA takes at least two core
			// clock cycles". The data moves right away, the transfer completes (DE, DTD, interrupt) after that time.
			// The JIT only runs the peripherals between its blocks, so data that arrived at the end of the time would
			// be too late for firmware that sets DE and uses the data further down the same interrupt handler.
			// The data arrives earlier than on the chip, which only matters to firmware that races the DMA
			m_dataTransferred = execTransfer();

			m_pendingTransfer = std::max(1, static_cast<int32_t>((m_dco + 1) << 1));
			m_peripherals.setDelayCycles(m_pendingTransfer);
			m_lastClock = m_peripherals.getDSP().getInstructionCounter();
			return;
		}

		const auto tm = getTransferMode();
		const auto reqSrc = getRequestSource();

		const auto isSupportedTransferMode = tm == TransferMode::WordTriggerRequest || tm == TransferMode::WordTriggerRequestClearDE || tm == TransferMode::LineTriggerRequestClearDE;

		if(!isSupportedTransferMode)
		{
			assert(false && "TODO implement transfer mode in execTransfer()");
			return;
		}

		if(m_peripherals.getType() == PeripheralType::Peripherals56303)
		{
			auto* p303 = static_cast<Peripherals56303*>(&m_peripherals);
			m_dma.addTriggerTarget(this);
			m_armed = true;
			if(checkTrigger(*p303, reqSrc))
				triggerByRequest();
		}
		else if(m_peripherals.getType() == PeripheralType::Peripherals56362)
		{
			auto* p362 = static_cast<Peripherals56362*>(&m_peripherals);
			m_dma.addTriggerTarget(this);
			m_armed = true;
			if(checkTrigger(*p362, reqSrc))
				triggerByRequest();
		}
		else
		{
			assert(false && "TODO unknown peripherals, not supported yet");
		}

		m_peripherals.setDelayCycles(0);
	}

	const TWord& DmaChannel::getDSR() const
	{
		return m_dsr;
	}

	const TWord& DmaChannel::getDDR() const
	{
		return m_ddr;
	}

	const TWord& DmaChannel::getDCO() const
	{
		return m_dco;
	}

	const TWord& DmaChannel::getDCR() const
	{
		return m_dcr;
	}

	uint32_t DmaChannel::exec()
	{
		if(m_pendingTransfer <= 0)
			return IPeripherals::MaxDelayCycles;

		const auto clock = m_peripherals.getDSP().getInstructionCounter();
		const auto diff = clock - m_lastClock;
		m_lastClock = clock;

		m_pendingTransfer -= static_cast<int32_t>(diff);

		if(m_pendingTransfer <= 0)
		{
			if(m_dataTransferred || execTransfer())
			{
				m_pendingTransfer = 0;
				m_dataTransferred = false;
				finishTransfer();
			}
			else
			{
				m_pendingTransfer = 1;
			}
		}

		return m_pendingTransfer;
	}

	void DmaChannel::triggerByRequest()
	{
		if(!bittest(m_dcr, De))
			return;

		// Skip transfer if DSR/DDR haven't been written yet. The firmware
		// may enable DMA (set DE) before configuring source/destination.
		// On real hardware, no trigger fires in that window because the
		// ESAI clock is synchronous with the CPU. In the emulator,
		// peripheral ticks between JIT blocks can trigger DMA before
		// the firmware finishes configuring it.
		if(!m_dsrWritten || !m_ddrWritten)
			return;

		if(execTransfer())
			finishTransfer();
	}

	DmaChannel::TransferMode DmaChannel::getTransferMode() const
	{
		return static_cast<TransferMode>((m_dcr >> 19) & 7);
	}

	TWord DmaChannel::getPriority() const
	{
		return (m_dcr >> 17) & 3;
	}

	DmaChannel::RequestSource DmaChannel::getRequestSource() const
	{
		return static_cast<RequestSource>((m_dcr >> 11) & 0x1f);
	}

	TWord DmaChannel::getAddressMode() const
	{
		return (m_dcr >> 4) & 0x3f;
	}

	EMemArea DmaChannel::getSourceSpace() const
	{
		return convertMemArea(m_dcr & 3);
	}

	EMemArea DmaChannel::getDestinationSpace() const
	{
		return convertMemArea((m_dcr >> 2) & 3);
	}

	bool DmaChannel::isRequestTrigger() const
	{
		return getTransferMode() != TransferMode::BlockTriggerDEClearDE;
	}

	bool DmaChannel::isDEClearedAfterTransfer() const
	{
		return static_cast<uint32_t>(getTransferMode()) <= 3;
	}

	EMemArea DmaChannel::convertMemArea(const TWord _space)
	{
		switch (_space)
		{
		case 0:		return MemArea_X;
		case 1:		return MemArea_Y;
		case 2:		return MemArea_P;
		default:	return MemArea_COUNT;
		}
	}

	DmaChannel::AddressGenMode DmaChannel::getSourceAddressGenMode() const
	{
		return static_cast<AddressGenMode>((m_dcr >> 4) & 7);
	}

	DmaChannel::AddressGenMode DmaChannel::getDestinationAddressGenMode() const
	{
		return static_cast<AddressGenMode>((m_dcr >> 7) & 7);
	}

	TWord DmaChannel::getDAM() const
	{
		return (m_dcr >> 4) & 0x3f;
	}

	void DmaChannel::memCopy(EMemArea _dstArea, TWord _dstAddr, EMemArea _srcArea, TWord _srcAddr, TWord _count) const
	{
		if(_dstAddr >= m_peripherals.getDSP().memory().getBridgedMemoryAddress())
			_dstArea = MemArea_P;

		auto copyIndividual = [&]()
		{
			auto srcA = _srcAddr;
			auto dstA = _dstAddr;

			for (TWord i = 0; i < _count; ++i)
			{
				const TWord data = memRead(_srcArea, srcA);
				memWrite(_dstArea, dstA, data);
				++srcA;
				++dstA;
			}
		};

		if(_dstArea == MemArea_P || isPeripheralAddr(_dstArea, _dstAddr, _count) || isPeripheralAddr(_srcArea, _srcAddr, _count))
		{
			copyIndividual();
		}
		else
		{
//			auto& dsp = m_peripherals.getDSP();
//			auto& mem = dsp.memory();

			if (bridgedOverlap(_dstArea, _dstAddr, _count) || bridgedOverlap(_srcArea, _srcAddr, _count))
			{
				copyIndividual();
			}
			else
			{
				const auto* src = getMemPtr(_srcArea, _srcAddr);
				auto* dst = getMemPtr(_dstArea, _dstAddr);

				memcpy(dst, src, sizeof(TWord) * _count);
			}
		}
	}

	void DmaChannel::memFill(EMemArea _dstArea, TWord _dstAddr, EMemArea _srcArea, TWord _srcAddr, TWord _count) const
	{
		const auto readMultiple = isPeripheralAddr(_srcArea, _srcAddr);

		if(_dstAddr >= m_peripherals.getDSP().memory().getBridgedMemoryAddress())
			_dstArea = MemArea_P;

		const auto writeIndividual = _dstArea == MemArea_P || isPeripheralAddr(_dstArea, _dstAddr, _count) || bridgedOverlap(_dstArea, _dstAddr, _count);

		if (readMultiple)
		{
			auto dstA = _dstAddr;

			for (TWord i = 0; i < _count; ++i)
			{
				const TWord data = memRead(_srcArea, _srcAddr);
				memWrite(_dstArea, dstA, data);
				++dstA;
			}
		}
		else
		{
			const auto data = memRead(_srcArea, _srcAddr);

			if(writeIndividual)
			{
				auto dstA = _dstAddr;

				for (TWord i = 0; i < _count; ++i)
					memWrite(_dstArea, dstA++, data);
			}
			else
			{
				auto* dst = getMemPtr(_dstArea, _dstAddr);

				for (TWord i = 0; i < _count; ++i)
					*dst++ = data;
			}
		}
	}

	void DmaChannel::memCopyToFixedDest(EMemArea _dstArea, TWord _dstAddr, EMemArea _srcArea, TWord _srcAddr, TWord _count) const
	{
		TWord srcAddr = _srcAddr;

		for (TWord i = 0; i < _count; ++i)
		{
			const TWord data = memRead(_srcArea, srcAddr);
			memWrite(_dstArea, _dstAddr, data);
			++srcAddr;
		}
	}

	bool DmaChannel::dualModeIncrement(TWord& _dst, const TWord _dor)
	{
		if(m_dcol > 0)
		{
			--m_dcol;
			++_dst;
		}
		else if(m_dcoh > 0)
		{
			_dst += _dor;
			m_dcol = m_dcolInit;
			--m_dcoh;
		}
		else
		{
			_dst += _dor;
			_dst &= 0xffffff;
			return true;
		}

		_dst &= 0xffffff;
		return false;
	}

	bool DmaChannel::isPeripheralAddr(const EMemArea _area, const TWord _first, const TWord _count) const
	{
		if (_area == MemArea_P)
			return false;
		if (m_peripherals.getDSP().isPeripheralAddress(_first))
			return true;
		if (!m_peripherals.getDSP().isPeripheralAddress(_first + _count - 1))
			return false;
		return true;
	}

	bool DmaChannel::isPeripheralAddr(const EMemArea _area, const TWord _addr) const
	{
		return _area != MemArea_P && m_peripherals.getDSP().isPeripheralAddress(_addr);
	}

	bool DmaChannel::bridgedOverlap(EMemArea _area, TWord _first, TWord _count) const
	{
		if (_area == MemArea_P)
			return false;

		const auto& mem = m_peripherals.getDSP().memory();

		if (_first + _count <= mem.getBridgedMemoryAddress())
			return false;
		if (_first >= mem.getBridgedMemoryAddress())
			return false;
		return true;
	}

	void DmaChannel::extractDCOHML(TWord& _h, TWord& _m, TWord& _l) const
	{
		const auto dam = getDAM();
		const auto counterMode = dam & 3;

		switch (counterMode)
		{
		case 0b00:
			_h = (m_dco >> 12) & 0xfff;
			_m = (m_dco >> 6) & 0x3f;
			_l = (m_dco) & 0x3f;
			break;
		case 0b01:
			_h = (m_dco >> 18) & 0x3ff;
			_m = (m_dco >> 6) & 0xfff;
			_l = (m_dco) & 0x3f;
			break;
		case 0b10:
			_h = (m_dco >> 18) & 0x3ff;
			_m = (m_dco >> 12) & 0x3f;
			_l = (m_dco) & 0xffff;
			break;
		default:
			// reserved
			break;
		}
	}

	TWord DmaChannel::memRead(EMemArea _area, TWord _addr) const
	{
		auto& dsp = m_peripherals.getDSP();
		if (isPeripheralAddr(_area, _addr))
			return dsp.getPeriph(_area)->read(_addr | 0xff0000, Nop);
		return dsp.memory().get(_area, _addr);
	}

	void DmaChannel::memWrite(EMemArea _area, TWord _addr, TWord _value) const
	{
		auto& dsp = m_peripherals.getDSP();
		if (isPeripheralAddr(_area, _addr))
			dsp.getPeriph(_area)->write(_addr | 0xff0000, _value);
		else if (_area == MemArea_P)
			dsp.memWriteP(_addr, _value);
		else
			dsp.memWrite(_area, _addr, _value);
	}

	TWord* DmaChannel::getMemPtr(EMemArea _area, TWord _addr) const
	{
		auto& mem = m_peripherals.getDSP().memory();

		TWord* basePtr;
		if (_area == MemArea_P || _addr >= mem.getBridgedMemoryAddress())
			basePtr = mem.getMemAreaPtr(MemArea_P);
		else
			basePtr = mem.getMemAreaPtr(_area);
		return basePtr + _addr;
	}

	bool DmaChannel::execTransfer()
	{
		const auto areaS = getSourceSpace();
		const auto areaD = getDestinationSpace();

		if (areaS == MemArea_COUNT || areaD == MemArea_COUNT)
			return true;

		if(bitvalue(m_dcr, D3d))
		{
			memWrite(areaD, m_ddr, memRead(areaS, m_dsr));

			const auto dam = getDAM();
			const auto addressGenMode = (dam >> 3) & 7;
			const auto addrModeSelect = (dam >> 2) & 1;

			const auto offsetA = signextend<int, 24>(static_cast<int>(m_dma.getDOR(addrModeSelect << 1)));
			const auto offsetB = signextend<int, 24>(static_cast<int>(m_dma.getDOR((addrModeSelect << 1) + 1)));

			auto blockFinished = false;

			auto increment = [&](TWord& _target)
			{
				const auto prev = _target;

				if(m_dcol == 0)
				{
					m_dcol = m_dcolInit;

					if(m_dcom == 0)
					{
						m_dcom = m_dcomInit;

						if(m_dcoh == 0)
						{
							m_dcoh = m_dcohInit;
							blockFinished = true;
						}
						else
						{
							--m_dcoh;
						}

						_target += offsetB;
					}
					else
					{
						--m_dcom;
						_target += offsetA;
					}
				}
				else
				{
					_target++;
					--m_dcol;
				}

				_target &= 0xffffff;
//				LOG("DMA" << m_index << " address change " << HEX(prev) << " => " << HEX(_target));
			};

			if(addressGenMode == 4)	// No Update
			{
				if(addrModeSelect == 0)	// Source: Three-Dimensional / Destination: Defined by DAM[5-3]
					increment(m_dsr);
				else					// the other way around
					increment(m_ddr);
				return blockFinished;
			}

			assert(false && "three-dimensional DMA modes are not supported yet");

			return blockFinished;
		}

		const auto agmS = getSourceAddressGenMode();
		const auto agmD = getDestinationAddressGenMode();

		if (agmS == AddressGenMode::SingleCounterApostInc && agmD == AddressGenMode::SingleCounterApostInc)
		{
			assert(!isRequestTrigger() && "not supported yet, needs to be transfer one word at a time");
			memCopy(areaD, m_ddr, areaS, m_dsr, m_dco + 1);
			m_dsr += m_dco + 1;
			m_ddr += m_dco + 1;
			return true;
		}

		if (agmS == AddressGenMode::SingleCounterAnoUpdate && agmD == AddressGenMode::SingleCounterApostInc)
		{
			// can be used to continously read a peripheral and write to a memory region
			if(isRequestTrigger())
			{
				memWrite(areaD, m_ddr, memRead(areaS, m_dsr));
				++m_ddr;
				if(m_dco)
				{
					--m_dco;
					return false;
				}

				m_dco = m_dcomInit;
				return true;
			}

			memFill(areaD, m_ddr, areaS, m_dsr, m_dco + 1);
			m_ddr += m_dco + 1;
			return true;
		}

		if (agmS == AddressGenMode::SingleCounterAnoUpdate && agmD == AddressGenMode::SingleCounterAnoUpdate)
		{
			// a peripheral into a fixed location, or the other way around
			if(isRequestTrigger())
			{
				memWrite(areaD, m_ddr, memRead(areaS, m_dsr));
				if(m_dco)
				{
					--m_dco;
					return false;
				}

				m_dco = m_dcomInit;
				return true;
			}

			for(TWord i=0; i<=m_dco; ++i)
				memWrite(areaD, m_ddr, memRead(areaS, m_dsr));
			return true;
		}

		if(agmS == AddressGenMode::SingleCounterApostInc && agmD == AddressGenMode::SingleCounterAnoUpdate)
		{
			// can be used to continously feed a peripheral from a memory region
			if(isRequestTrigger())
			{
				memWrite(areaD, m_ddr, memRead(areaS, m_dsr));
				++m_dsr;
				if(m_dco)
				{
					--m_dco;
					return false;
				}

				m_dco = m_dcomInit;
				return true;
			}

			memCopyToFixedDest(areaD, m_ddr, areaS, m_dsr, m_dco + 1);
			m_dsr += m_dco + 1;
			return true;
		}

		if(agmS == AddressGenMode::SingleCounterApostInc && agmD == AddressGenMode::DualCounterDOR1)
		{
			// 2D mode, can be either line or word

			const auto tm = getTransferMode();
			const auto isLineTransfer = tm == TransferMode::LineTriggerRequestClearDE;

			do
			{
				memWrite(areaD, m_ddr, memRead(areaS, m_dsr));
				++m_dsr;

				if(dualModeIncrement(m_ddr, m_dma.getDOR(1)))
					return true;
			}
			while(isLineTransfer && m_dcol != m_dcolInit);

			return false;
		}

		if(agmS <= AddressGenMode::DualCounterDOR3 || agmD <= AddressGenMode::DualCounterDOR3)
		{
			// Two-dimensional on either side, counter mode B. Each word moves a two-dimensional address by one, or by
			// its offset register after the last word of a line; the other side updates as its own mode says
			auto advance = [this](TWord& _addr, const AddressGenMode _mode, const bool _lineEnd)
			{
				if(_mode <= AddressGenMode::DualCounterDOR3)
					_addr += _lineEnd ? m_dma.getDOR(static_cast<TWord>(_mode)) : 1;
				else if(_mode == AddressGenMode::SingleCounterApostInc)
					++_addr;
				_addr &= 0xffffff;
			};

			const auto tm = getTransferMode();
			const auto isWordTransfer = tm == TransferMode::WordTriggerRequest || tm == TransferMode::WordTriggerRequestClearDE;
			const auto isLineTransfer = tm == TransferMode::LineTriggerRequestClearDE;

			while(true)
			{
				memWrite(areaD, m_ddr, memRead(areaS, m_dsr));

				const auto lineEnd = m_dcol == 0;
				const auto blockEnd = lineEnd && m_dcoh == 0;

				advance(m_dsr, agmS, lineEnd);
				advance(m_ddr, agmD, lineEnd);

				if(!lineEnd)
				{
					--m_dcol;
				}
				else
				{
					m_dcol = m_dcolInit;
					if(blockEnd)
						m_dcoh = m_dcohInit;
					else
						--m_dcoh;
				}

				if(blockEnd)
					return true;
				if(isWordTransfer || (isLineTransfer && lineEnd))
					return false;
			}
		}

		assert(false && "DMA transfer mode not supported yet");
		return true;
	}

	void DmaChannel::finishTransfer()
	{
		if(isDEClearedAfterTransfer())
			m_dcr &= ~(1 << De);

		m_dma.clearActiveChannel();

		if(bitvalue(m_dcr, Die))
			m_peripherals.getDSP().injectInterrupt(Vba_DMAchannel0 + (m_index<<1));
	}

	Dma::Dma(IPeripherals& _peripherals)
		: m_dstr((1 << Dtd0) | (1 << Dtd1) | (1 << Dtd2) | (1 << Dtd3) | (1 << Dtd4) | (1 << Dtd5))
		, m_channels({
			  DmaChannel(*this, _peripherals, 0),
			  DmaChannel(*this, _peripherals, 1),
			  DmaChannel(*this, _peripherals, 2),
			  DmaChannel(*this, _peripherals, 3),
			  DmaChannel(*this, _peripherals, 4),
			  DmaChannel(*this, _peripherals, 5)
		  })
		, m_dor{0,0,0,0}
	{
		for (auto& requestTarget : m_requestTargets)
			requestTarget.reserve(m_channels.size());
	}
	/*
	void Dma::setDSTR(const TWord _value)
	{
		m_dstr = _value;
		LOGDMA("DMA set DSTR" << " = " << HEX(_value));
	}
	*/
	const TWord& Dma::getDSTR() const
	{
		return m_dstr;
	}

	void Dma::setDOR(const TWord _index, const TWord _value)
	{
		m_dor[_index] = _value;
		LOGDMA("DMA set DOR" << _index << " = " << HEX(_value));
	}

	TWord Dma::getDOR(const TWord _index) const
	{
		return m_dor[_index];
	}

	uint32_t Dma::exec() noexcept
	{
		if((m_dstr & (1 << Dact)) == 0)
			return IPeripherals::MaxDelayCycles;

		auto delay = m_channels[0].exec();

		delay = std::min(delay, m_channels[1].exec());
		delay = std::min(delay, m_channels[2].exec());
		delay = std::min(delay, m_channels[3].exec());
		delay = std::min(delay, m_channels[4].exec());
		delay = std::min(delay, m_channels[5].exec());

		return delay;
	}

	void Dma::setActiveChannel(const TWord _channel)
	{
		m_dstr |= (1 << Dact);

		m_dstr &= ~DchMask;
		m_dstr |= _channel << Dch0;
	}

	void Dma::clearActiveChannel()
	{
		// exec only runs while a channel is active. A transfer that completes right away must not hide the delayed
		// transfer of another channel, which would never finish then and keep its DE set
		for (const auto& channel : m_channels)
		{
			if(channel.hasPendingTransfer())
			{
				setActiveChannel(channel.getIndex());
				return;
			}
		}

		m_dstr &= ~(1 << Dact);
	}

	bool Dma::hasTrigger(DmaChannel::RequestSource _source) const
	{
		const auto& channels = m_requestTargets[static_cast<uint32_t>(_source)];
		return !channels.empty();
	}

	bool Dma::trigger(DmaChannel::RequestSource _source) const
	{
		const auto& channels = m_requestTargets[static_cast<uint32_t>(_source)];

		if (channels.empty())
			return false;

		for (auto& channel : channels)
			channel->triggerByRequest();

		return true;
	}

	void Dma::addTriggerTarget(DmaChannel* _channel)
	{
		auto src = _channel->getRequestSource();

		auto& channels = m_requestTargets[static_cast<uint32_t>(src)];

		channels.push_back(_channel);
	}

	void Dma::removeTriggerTarget(const DmaChannel* _channel)
	{
		auto src = _channel->getRequestSource();

		auto& channels = m_requestTargets[static_cast<uint32_t>(src)];

		auto it = std::find(channels.begin(), channels.end(), _channel);
		if (it != channels.end())
			channels.erase(it);
	}
}
