#pragma once
#include <array>
#include <set>

#include "types.h"

namespace dsp56k
{
	class Dma;
	class IPeripherals;

	class DmaChannel
	{
	public:
		enum DcrBits
		{
			Dss0, Dss1,								// DMA Source Space
			Dds0, Dds1,								// DMA Destination Space
			Dam0, Dam1, Dam2, Dam3, Dam4, Dam5,		// DMA Address Mode
			D3d,									// Three-Dimensional Mode
			Drs0, Drs1, Drs2, Drs3, Drs4,			// DMA Request Source
			Dcon,									// DMA Continuous Mode Enable
			Dpr0, Dpr1,								// DMA Channel Priority
			Dtm0, Dtm1, Dtm2,						// DMA Transfer Mode
			Die,									// DMA Interrupt Enable
			De										// DMA Channel Enable
		};

		enum class AddressGenMode
		{
			DualCounterDOR0,
			DualCounterDOR1,
			DualCounterDOR2,
			DualCounterDOR3,
			SingleCounterAnoUpdate,
			SingleCounterApostInc,
			reserved110,
			reserved111
		};

		enum class CounterType
		{
			CounterA,
			CounterB,
			CounterC,
			CounterD,
			CounterE
		};

		enum class RequestSource
		{
			ExternalIRQA       = 0b00000, // External (IRQA pin)
			ExternalIRQB       = 0b00001, // External (IRQB pin)
			ExternalIRQC       = 0b00010, // External (IRQC pin)
			ExternalIRQD       = 0b00011, // External (IRQD pin)
			DMAChannel0        = 0b00100, // Transfer done from DMA channel 0
			DMAChannel1        = 0b00101, // Transfer done from DMA channel 1
			DMAChannel2        = 0b00110, // Transfer done from DMA channel 2
			DMAChannel3        = 0b00111, // Transfer done from DMA channel 3
			DMAChannel4        = 0b01000, // Transfer done from DMA channel 4
			DMAChannel5        = 0b01001, // Transfer done from DMA channel 5

			// DSP56362
			DaxTransmitData    = 0b01010, // DAX transmit data
			EsaiReceiveData    = 0b01011, // ESAI receive data (RDF=1)
			EsaiTransmitData   = 0b01100, // ESAI transmit data (TDE=1)
			ShiHtxEmpty        = 0b01101, // SHI HTX empty
			ShiFifoNotEmpty    = 0b01110, // SHI FIFO not empty
			ShiFifoFull        = 0b01111, // SHI FIFO full
			HostReceiveData    = 0b10000, // Host receive data
			HostTransmitData   = 0b10001, // Host transmit data
			Timer0             = 0b10010, // TIMER0 (TCF=1)
			Timer1             = 0b10011, // TIMER1 (TCF=1)
			Timer2             = 0b10100, // TIMER2 (TCF=1)

			Count,

			// DSP56303
			Essi0ReceiveData   = 0b01010, // ESSI0 receive data (RDF0 = 1)
			Essi0TransmitData  = 0b01011, // ESSI0 transmit data (TDE0 = 1))
			Essi1ReceiveData   = 0b01100, // ESSI1 receive data (RDF1 = 1)
			Essi1TransmitData  = 0b01101, // ESSI1 transmit data (TDE1 = 1))
		};

		enum class TransferMode
		{
			BlockTriggerRequestClearDE,		// Mode = Block, Trigger = Request, Clear DE = yes
			WordTriggerRequestClearDE,		// Mode = Word, Trigger = Request, Clear DE = yes
			LineTriggerRequestClearDE,		// Mode = Line, Trigger = Request, Clear DE = yes
			BlockTriggerDEClearDE,			// Mode = Block, Trigger = DE, Clear DE = yes
			BlockTriggerRequest,			// Mode = Block, Trigger = Request, Clear DE = no
			WordTriggerRequest,				// Mode = Word, Trigger = Request, Clear DE = no
		};

		DmaChannel(Dma& _dma, IPeripherals& _peripherals, TWord _index);

		void setDSR(TWord _address);
		void setDDR(TWord _address);
		void setDCO(TWord _count);
		void setDCR(TWord _controlRegister);

		const TWord& getDSR() const;
		const TWord& getDDR() const;
		const TWord& getDCO() const;
		const TWord& getDCR() const;

		uint32_t exec();

		void triggerByRequest();

		TransferMode getTransferMode() const;
		TWord getPriority() const;
		RequestSource getRequestSource() const;
		TWord getAddressMode() const;
		EMemArea getSourceSpace() const;
		EMemArea getDestinationSpace() const;
		bool isRequestTrigger() const;
		bool isDEClearedAfterTransfer() const;
		AddressGenMode getSourceAddressGenMode() const;
		AddressGenMode getDestinationAddressGenMode() const;
		TWord getDAM() const;

		static EMemArea convertMemArea(TWord _space);

		bool isPeripheralAddr(EMemArea _area, TWord _first, TWord _count) const;
		bool isPeripheralAddr(EMemArea _area, TWord _addr) const;

		bool bridgedOverlap(EMemArea _area, TWord _first, TWord _count) const;

		void extractDCOHML(TWord& __h, TWord& _m, TWord& _l) const;

	private:
		void memCopy(EMemArea _dstArea, TWord _dstAddr, EMemArea _srcArea, TWord _srcAddr, TWord _count) const;
		void memFill(EMemArea _dstArea, TWord _dstAddr, EMemArea _srcArea, TWord _srcAddr, TWord _count) const;
		void memCopyToFixedDest(EMemArea _dstArea, TWord _dstAddr, EMemArea _srcArea, TWord _srcAddr, TWord _count) const;

		bool dualModeIncrement(TWord& _dst, TWord _dor);

		TWord memRead(EMemArea _area, TWord _addr) const;
		void memWrite(EMemArea _area, TWord _addr, TWord _value) const;

		TWord* getMemPtr(EMemArea _area, TWord _addr) const;

		bool execTransfer();
		void finishTransfer();

		const TWord m_index;

		Dma& m_dma;
		IPeripherals& m_peripherals;

		TWord m_dsr = 0;
		TWord m_ddr = 0;
		TWord m_dco = 0;
		TWord m_dcr = 0;

		TWord m_dcoh = 0;
		TWord m_dcom = 0;
		TWord m_dcol = 0;

		TWord m_dcohInit = 0;
		TWord m_dcomInit = 0;
		TWord m_dcolInit = 0;

		int32_t m_pendingTransfer = 0;
		uint64_t m_lastClock = 0;
	};

	class Dma
	{
	public:
		enum DstrBits
		{
			Dtd0, Dtd1, Dtd2, Dtd3, Dtd4, Dtd5,		// DMA Transfer Done
			DstrUnusedBit6, DstrUnusedBit7,
			Dact,									// DMA Active
			Dch0, Dch1, Dch2,						// DMA Active Channel

			DchMask = (1<<Dch0) | (1<<Dch1) | (1<<Dch2)
		};

		using ChannelList = std::set<DmaChannel*>;

		Dma(IPeripherals& _peripherals);

//		void setDSTR(TWord _value);
		const TWord& getDSTR() const;

		void setDOR(TWord _index, TWord _value);
		TWord getDOR(TWord _index) const;

		void setDSR(const TWord _channel, const TWord _address) { m_channels[_channel].setDSR(_address); }
		void setDDR(const TWord _channel, const TWord _address) { m_channels[_channel].setDDR(_address); }
		void setDCO(const TWord _channel, const TWord _count) { m_channels[_channel].setDCO(_count); }
		void setDCR(const TWord _channel, const TWord _controlRegister) { m_channels[_channel].setDCR(_controlRegister); }

		const TWord& getDSR(const TWord _channel) const { return m_channels[_channel].getDSR(); }
		const TWord& getDDR(const TWord _channel) const { return m_channels[_channel].getDDR(); }
		const TWord& getDCO(const TWord _channel) const { return m_channels[_channel].getDCO(); }
		const TWord& getDCR(const TWord _channel) const { return m_channels[_channel].getDCR(); }

		uint32_t exec();
		void setActiveChannel(TWord _channel);
		void clearActiveChannel();

		bool trigger(DmaChannel::RequestSource _source);
		void addTriggerTarget(DmaChannel* _channel);
		void removeTriggerTarget(DmaChannel* _channel);

	private:
		TWord m_dstr;
		std::array<DmaChannel, 6> m_channels;
		std::array<TWord, 4> m_dor{};
		std::array<ChannelList, static_cast<uint32_t>(DmaChannel::RequestSource::Count)> m_requestTargets;
	};
}
