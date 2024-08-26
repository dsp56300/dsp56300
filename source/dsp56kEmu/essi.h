// DSP 56303 ESSI - Enhanced Synchronous Serial Interface

#pragma once

#include "esxi.h"
#include "types.h"

#include <string>

namespace dsp56k
{
	class Peripherals56303;
	class Disassembler;
	class DSP;
	class Memory;

	class Essi : public Esxi
	{
	public:
		// ESSI Control Register A (CRA)
		enum RegCRAbits
		{
			CRA_PM0,				// Prescale Modulus Select
			CRA_PM1,
			CRA_PM2,
			CRA_PM3,
			CRA_PM4,
			CRA_PM5,
			CRA_PM6,
			CRA_PM7,
			CRA_Reserved_08,		// Reserved. Write to 0 for future compatibility
			CRA_Reserved_09,
			CRA_Reserved_10,
			CRA_PSR,				// Prescaler Range
			CRA_DC0,				// Frame Rate Divider Control
			CRA_DC1,
			CRA_DC2,
			CRA_DC3,
			CRA_DC4,
			CRA_Reserved_17,		// Reserved. Write to 0 for future compatibility.
			CRA_ALC,				// Alignment Control
			CRA_WL0,				// Word Length Control
			CRA_WL1,
			CRA_WL2,
			CRA_SSC1,				// Select SC1
			CRA_Reserved_23,		// Reserved. Write to 0 for future compatibility.

			CRA_PM = 0xff,
			CRA_DC = 0x1f000,
			CRA_WL = 0x380000,
		};

		// ESSI Control Register B (CRB)
		enum RegCRBbits
		{
			CRB_OF0,				// Serial Out Flag 0
			CRB_OF1,				// Serial Out Flag 1
			CRB_SCD0,				// Serial Control Direction 0
			CRB_SCD1,				// Serial Control Direction 1
			CRB_SCD2,				// Serial Control Direction 2
			CRB_SCKD,				// Clock Source Direction
			CRB_SHFD,				// Shift Direction
			CRB_FSL0,				// Frame Sync Length
			CRB_FSL1,
			CRB_FSR,				// Frame Sync Relative Timing
			CRB_FSP,				// Frame Sync Polarity
			CRB_CKP,				// Clock Polarity
			CRB_SYN,				// Synchronous/Asynchronous
			CRB_MOD,				// Mode Select
			CRB_TE2,				// Transmit 2 Enable
			CRB_TE1,				// Transmit 1 Enable
			CRB_TE0,				// Transmit 0 Enable
			CRB_RE,					// Receive Enable
			CRB_TIE,				// Transmit Interrupt Enable
			CRB_RIE,				// Receive Interrupt Enable
			CRB_TLIE,				// Transmit Last Slot Interrupt Enable
			CRB_RLIE,				// Receive Last Slot Interrupt Enable
			CRB_TEIE,				// Transmit Exception Interrupt Enable
			CRB_REIE,				// Receive Exception Interrupt Enable

			CRB_OF = 0x3,
			CRB_SCD = 0x1c,
			CRB_FSL = 0x180,
			CRB_TE = 0x1c000,
		};

		enum RegSSISRbits
		{
			SSISR_IF0,				// Serial Input Flag 0
			SSISR_IF1,				// Serial Input Flag 1
			SSISR_TFS,				// Transmit Frame Sync Flag
			SSISR_RFS,				// Receive Frame Sync Flag
			SSISR_TUE,				// Transmitter Underrun Error Flag
			SSISR_ROE,				// Receiver Overrun Error Flag
			SSISR_TDE,				// Transmit Data Register Empty
			SSISR_RDF,				// Receive Data Register Full
			SSISR_Reserved_08,		// Reserved. Write to 0 for future compatibility.
			SSISR_Reserved_09,
			SSISR_Reserved_10,
			SSISR_Reserved_11,
			SSISR_Reserved_12,
			SSISR_Reserved_13,
			SSISR_Reserved_14,
			SSISR_Reserved_15,
			SSISR_Reserved_16,
			SSISR_Reserved_17,
			SSISR_Reserved_18,
			SSISR_Reserved_19,
			SSISR_Reserved_20,
			SSISR_Reserved_21,
			SSISR_Reserved_22,
			SSISR_Reserved_23,

			SSISR_IF = 0x3
		};

		enum EssiRegX
		{
			// ESSI 1
			ESSI1_RSMB	= 0xffffa1,		// Receive Slot Mask Register B
			ESSI1_RSMA	= 0xffffa2,		// Receive Slot Mask Register A
			ESSI1_TSMB	= 0xffffa3,		// Transmit Slot Mask Register B
			ESSI1_TSMA	= 0xffffa4,		// Transmit Slot Mask Register A
			ESSI1_CRA	= 0xffffa5,		// Control Register A
			ESSI1_CRB	= 0xffffa6,		// Control Register B
			ESSI1_SSISR	= 0xffffa7,		// Status Register
			ESSI1_RX	= 0xffffa8,		// Receive Data Register
			ESSI1_TSR	= 0xffffa9,		// Time Slot Register
			ESSI1_TX2	= 0xffffaa,		// Transmit Data Register 2
			ESSI1_TX1	= 0xffffab,		// Transmit Data Register 1
			ESSI1_TX0	= 0xffffac,		// Transmit Data Register 0

			// GPIO Port D
			ESSI_PDRD	= 0xffffad,		// Port D GPIO Data Register
			ESSI_PRRD	= 0xffffae,		// Port D Direction Register
			ESSI_PCRD	= 0xffffaf,		// Port D Control Register
			
			// ESSI 0
			ESSI0_RSMB	= 0xffffb1,		// Receive Slot Mask Register B
			ESSI0_RSMA	= 0xffffb2,		// Receive Slot Mask Register A
			ESSI0_TSMB	= 0xffffb3,		// Transmit Slot Mask Register B
			ESSI0_TSMA	= 0xffffb4,		// Transmit Slot Mask Register A
			ESSI0_CRA	= 0xffffb5,		// Control Register A
			ESSI0_CRB	= 0xffffb6,		// Control Register B
			ESSI0_SSISR	= 0xffffb7,		// Status Register
			ESSI0_RX	= 0xffffb8,		// Receive Data Register
			ESSI0_TSR	= 0xffffb9,		// Time Slot Register
			ESSI0_TX2	= 0xffffba,		// Transmit Data Register 2
			ESSI0_TX1	= 0xffffbb,		// Transmit Data Register 1
			ESSI0_TX0	= 0xffffbc,		// Transmit Data Register 0

			// GPIO Port C
			ESSI_PDRC	= 0xffffbd,		// Port C GPIO Data Register
			ESSI_PRRC	= 0xffffbe,		// Port C Direction Register
			ESSI_PCRC	= 0xffffbf,		// Port C Control Register
		};


		static constexpr uint32_t EssiAddressOffset = ESSI0_RSMB - ESSI1_RSMB;

		// _____________________________________________________________________________
		// implementation
		//
		explicit Essi(Peripherals56303& _peripheral, uint32_t _essiIndex);

		void reset();

		void execTX() override;
		void execRX() override;

		TWord hasEnabledTransmitters() const override	{ return m_crb.testMask(RegCRBbits::CRB_TE); }
		TWord hasEnabledReceivers() const override		{ return m_crb.test(RegCRBbits::CRB_RE); }

		bool getReceiveFrameSync() const				{ return m_sr.test(SSISR_RFS); }
		bool getTransmitFrameSync() const				{ return m_sr.test(SSISR_TFS); }

		void setDSP(DSP* _dsp);

		void setSymbols(Disassembler& _disasm) const;

		TWord readTX(uint32_t _index);
		TWord readTSR();
		TWord readRX();
		const TWord& readSR() const;
		TWord readCRA();
		TWord readCRB();
		TWord readTSMA();
		TWord readTSMB();
		TWord readRSMA();
		TWord readRSMB();

		void writeTX(uint32_t _index, TWord _val);
		void writeTSR(TWord _val);
		void writeRX(TWord _val);
		void writeSR(TWord _val);
		void writeCRA(TWord _val);
		void writeCRB(TWord _val);
		void writeTSMA(TWord _val);
		void writeTSMB(TWord _val);
		void writeRSMA(TWord _val);
		void writeRSMB(TWord _val);
		
		std::string getCraAsString() const;
		std::string getCrbAsString() const;

		const auto& getSR() const { return m_sr; }

	private:
		TWord getRxWordCount() const;
		TWord getTxWordCount() const;

		void injectInterrupt(TWord _interrupt) const;
		void readSlotFromFrame();
		void writeSlotToFrame();

		void dmaTrigger(uint32_t _trigger) const;

		Peripherals56303& m_periph;
		const uint32_t m_index;

		TxSlot m_tx;
		TWord m_tsr = 0;
		RxSlot m_rx;
		Bitfield<uint32_t, RegSSISRbits, 8> m_sr;
		Bitfield<uint32_t, RegCRAbits, 24> m_cra;
		Bitfield<uint32_t, RegCRBbits, 24> m_crb;
		TWord m_tsma = 0;
		TWord m_tsmb = 0;
		TWord m_rsma = 0;
		TWord m_rsmb = 0;

		TWord m_vbaRead = 0;

		TWord m_rxSlotCounter = 0;
		TWord m_rxFrameCounter = 0;
		TWord m_txSlotCounter = 0;
		TWord m_txFrameCounter = 0;
		TWord m_readRX = 0;
		TWord m_writtenTX = 0;

		TxFrame m_txFrame;
		RxFrame m_rxFrame;
	};
}
