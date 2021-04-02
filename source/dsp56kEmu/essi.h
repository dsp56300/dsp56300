#pragma once

#include "types.h"

namespace dsp56k
{
	class DSP;
	class Memory;

	class Essi
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
			CRA_Reserved_23			// Reserved. Write to 0 for future compatibility.
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

		enum EssiIndex
		{
			Essi0 = 0x10,
			Essi1 = 0x00
		};

		// _____________________________________________________________________________
		// implementation
		//
		Essi( DSP& _dsp, Memory& _memory ) : m_dsp(_dsp), m_memory(_memory)
		{
		}

		void reset();
		void exec();
		void setControlRegisters(EssiIndex _essi, TWord cra, TWord crb);

	private:
		void reset(EssiIndex _index);

		static TWord address(EssiIndex _type, EssiRegX addr)
		{
			return (addr & (~Essi0)) | _type;
		}

		void set(EssiIndex _index, EssiRegX _reg, TWord _value);
		TWord get(EssiIndex _index, EssiRegX _reg) const;

		// _____________________________________________________________________________
		// members
		//
		DSP&		m_dsp;
		Memory&		m_memory;
	};
}
