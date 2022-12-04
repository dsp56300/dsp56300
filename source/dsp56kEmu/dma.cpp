#include "dma.h"

#include "dsp.h"
#include "logging.h"
#include "peripherals.h"
#include "utils.h"

#include <cstring> // memcpy

#include "interrupts.h"

#if 0
#define LOGDMA(S) LOG(S)
#else
#define LOGDMA(S) {}
#endif

constexpr bool g_delayedDmaTransfer = true;

namespace dsp56k
{
	DmaChannel::DmaChannel(Dma& _dma, IPeripherals& _peripherals, const TWord _index): m_index(_index), m_dma(_dma), m_peripherals(_peripherals)
	{
	}

	void DmaChannel::setDSR(const TWord _address)
	{
		m_dsr = _address;
		LOGDMA("DMA set DSR" << m_index << " = " << HEX(_address));
	}

	void DmaChannel::setDDR(const TWord _address)
	{
		m_ddr = _address;
		LOGDMA("DMA set DDR" << m_index << " = " << HEX(_address));
	}

	void DmaChannel::setDCO(const TWord _count)
	{
		m_dco = _count;
		LOGDMA("DMA set DCO" << m_index << " = " << HEX(_count));
	}

	void DmaChannel::setDCR(const TWord _controlRegister)
	{
		m_dma.removeTriggerTarget(this);

		m_dcr = _controlRegister;

		LOGDMA("DMA set DCR" << m_index << " = " << HEX(_controlRegister));

		if (!bitvalue(m_dcr, De))
			return;

		if(bitvalue(m_dcr, D3d))
		{
			extractDCOHML(m_dcoh, m_dcom, m_dcol);
			m_dcohInit = m_dcoh;
			m_dcomInit = m_dcom;
			m_dcolInit = m_dcol;
		}

		if (!isRequestTrigger())
		{
			m_dma.setActiveChannel(m_index);

			if constexpr(!g_delayedDmaTransfer)
			{
				execTransfer();
				finishTransfer();
			}
			else
			{
				// "When the needed resources are available, each word transfer performed by the DMA takes at least two core clock cycles"
				m_pendingTransfer = std::max(1, static_cast<int32_t>((m_dco + 1) << 1)); 
//				m_pendingTransfer = 1;
				m_lastClock = m_peripherals.getDSP().getInstructionCounter();
			}
		}
		else
		{
			const auto tm = getTransferMode();
			const auto prio = getPriority();
			const auto reqSrc = getRequestSource();
			const auto addrModeSrc = getSourceAddressGenMode();
			const auto addrModeDst = getDestinationAddressGenMode();
			const auto srcSpace = getSourceSpace();
			const auto dstSpace = getDestinationSpace();

			if(tm == TransferMode::WordTriggerRequest && reqSrc == RequestSource::EsaiTransmitData)
			{
				m_dma.addTriggerTarget(this);
			}
			else if(tm == TransferMode::WordTriggerRequest && reqSrc == RequestSource::EsaiReceiveData)
			{
				m_dma.addTriggerTarget(this);
			}
			else
			{
				assert(false && "TODO");
			}
		}
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

	void DmaChannel::exec()
	{
		if constexpr (!g_delayedDmaTransfer)
			return;

		if(m_pendingTransfer <= 0)
			return;

		const auto clock = m_peripherals.getDSP().getInstructionCounter();
		const auto diff = delta(clock, m_lastClock);
		m_lastClock = clock;

		m_pendingTransfer -= static_cast<int32_t>(diff);

		if(m_pendingTransfer <= 0)
		{
			m_pendingTransfer = 0;
			execTransfer();

//			m_pendingInterrupt = 1;
			finishTransfer();
		}
/*		else if(m_pendingInterrupt)
		{
			if(--m_pendingInterrupt == 0)
			{
				finishTransfer();
			}
		}
*/	}

	void DmaChannel::triggerByRequest()
	{
		execTransfer();
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

	bool DmaChannel::isPeripheralAddr(EMemArea _area, TWord _first, TWord _count)
	{
		if (_area == MemArea_P)
			return false;
		if (_first >= XIO_Reserved_High_First)
			return true;
		if ((_first + _count) <= XIO_Reserved_High_First)
			return false;
		return true;
	}

	bool DmaChannel::isPeripheralAddr(EMemArea _area, TWord _addr)
	{
		return _area != MemArea_P && _addr >= XIO_Reserved_High_First;
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
			return dsp.getPeriph(_area)->read(_addr, Nop);
		return dsp.memory().get(_area, _addr);
	}

	void DmaChannel::memWrite(EMemArea _area, TWord _addr, TWord _value) const
	{
		auto& dsp = m_peripherals.getDSP();
		if (isPeripheralAddr(_area, _addr))
			dsp.getPeriph(_area)->write(_addr, _value);
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

	void DmaChannel::execTransfer()
	{
		const auto areaS = getSourceSpace();
		const auto areaD = getDestinationSpace();

		if (areaS == MemArea_COUNT || areaD == MemArea_COUNT)
			return;

		if(bitvalue(m_dcr, D3d))
		{
			memWrite(areaD, m_ddr, memRead(areaS, m_dsr));

			const auto dam = getDAM();
			const auto addressGenMode = (dam >> 3) & 7;
			const auto addrModeSelect = (dam >> 2) & 1;

			const auto offsetA = signextend<int, 24>(static_cast<int>(m_dma.getDOR(addrModeSelect << 1)));
			const auto offsetB = signextend<int, 24>(static_cast<int>(m_dma.getDOR((addrModeSelect << 1) + 1)));

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
							m_dcoh = m_dcohInit;
						else
							--m_dcoh;

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
					--m_dcom;
				}

//				LOG("DMA" << m_index << " address change " << HEX(prev) << " => " << HEX(_target));
			};

			if(addressGenMode == 4)	// No Update
			{
				if(addrModeSelect == 0)	// Source: Three-Dimensional / Destination: Defined by DAM[5-3]
					increment(m_dsr);
				else					// the other way around
					increment(m_ddr);
				return;
			}

			assert(false && "three-dimensional DMA modes are not supported yet");
		}
		else
		{
			const auto agmS = getSourceAddressGenMode();
			const auto agmD = getDestinationAddressGenMode();

			if (agmS == AddressGenMode::SingleCounterApostInc && agmD == AddressGenMode::SingleCounterApostInc)
				memCopy(areaD, m_ddr, areaS, m_dsr, m_dco + 1);
			else if (agmS == AddressGenMode::SingleCounterAnoUpdate && agmD == AddressGenMode::SingleCounterApostInc)
				memFill(areaD, m_ddr, areaS, m_dsr, m_dco + 1);
			else
				assert(false && "counter modes not supported yet");
		}
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

	void Dma::exec()
	{
		if((m_dstr & (1 << Dact)) == 0)
			return;

		m_channels[0].exec();
		m_channels[1].exec();
		m_channels[2].exec();
		m_channels[3].exec();
		m_channels[4].exec();
		m_channels[5].exec();
	}

	void Dma::setActiveChannel(const TWord _channel)
	{
		m_dstr |= (1 << Dact);

		m_dstr &= ~DchMask;
		m_dstr |= _channel << Dch0;
	}

	inline void Dma::clearActiveChannel()
	{
		m_dstr &= ~(1 << Dact);
	}

	void Dma::trigger(DmaChannel::RequestSource _source)
	{
		const auto& channels = m_requestTargets[static_cast<uint32_t>(_source)];

		for (auto& channel : channels)
			channel->triggerByRequest();
	}

	void Dma::addTriggerTarget(DmaChannel* _channel)
	{
		auto src = _channel->getRequestSource();

		auto& channels = m_requestTargets[static_cast<uint32_t>(src)];

		channels.insert(_channel);
	}

	void Dma::removeTriggerTarget(DmaChannel* _channel)
	{
		auto src = _channel->getRequestSource();

		auto& channels = m_requestTargets[static_cast<uint32_t>(src)];

		channels.erase(_channel);
	}
}
