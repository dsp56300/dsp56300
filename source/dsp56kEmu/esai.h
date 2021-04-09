// DSP 56362 ESAI - Enhanced Serial AUDIO Interface

#pragma once

#include "audio.h"

namespace dsp56k
{
	class IPeripherals;

	class Esai : public Audio
	{
	public:
		enum Addresses
		{
			M_RSMB	= 0xFFFFBC, // ESAI Receive Slot Mask Register B (RSMB)
			M_RSMA	= 0xFFFFBB, // ESAI Receive Slot Mask Register A (RSMA)
			M_TSMB	= 0xFFFFBA, // ESAI Transmit Slot Mask Register B (TSMB)
			M_TSMA	= 0xFFFFB9, // ESAI Transmit Slot Mask Register A (TSMA)
			M_RCCR	= 0xFFFFB8, // ESAI Receive Clock Control Register (RCCR)
			M_RCR	= 0xFFFFB7, // ESAI Receive Control Register (RCR)
			M_TCCR	= 0xFFFFB6, // ESAI Transmit Clock Control Register (TCCR)
			M_TCR	= 0xFFFFB5, // ESAI Transmit Control Register (TCR)
			M_SAICR	= 0xFFFFB4, // ESAI Control Register (SAICR)
			M_SAISR	= 0xFFFFB3, // ESAI Status Register (SAISR)
			M_RX3	= 0xFFFFAB, // ESAI Receive Data Register 3 (RX3)
			M_RX2	= 0xFFFFAA, // ESAI Receive Data Register 2 (RX2)
			M_RX1	= 0xFFFFA9, // ESAI Receive Data Register 1 (RX1)
			M_RX0	= 0xFFFFA8, // ESAI Receive Data Register 0 (RX0)
			M_TSR	= 0xFFFFA6, // ESAI Time Slot Register (TSR)
			M_TX5	= 0xFFFFA5, // ESAI Transmit Data Register 5 (TX5)
			M_TX4	= 0xFFFFA4, // ESAI Transmit Data Register 4 (TX4)
			M_TX3	= 0xFFFFA3, // ESAI Transmit Data Register 3 (TX3)
			M_TX2	= 0xFFFFA2, // ESAI Transmit Data Register 2 (TX2)
			M_TX1	= 0xFFFFA1, // ESAI Transmit Data Register 1 (TX1)
			M_TX0	= 0xFFFFA0, // ESAI Transmit Data Register 0 (TX0)
		};

		enum Bits
		{
			// RSMB Register bits
			M_RS31 = 15,
			M_RS30 = 14,
			M_RS29 = 13,
			M_RS28 = 12,
			M_RS27 = 11,
			M_RS26 = 10,
			M_RS25 = 9,
			M_RS24 = 8,
			M_RS23 = 7,
			M_RS22 = 6,
			M_RS21 = 5,
			M_RS20 = 4,
			M_RS19 = 3,
			M_RS18 = 2,
			M_RS17 = 1,
			M_RS16 = 0,

			// RSMA Register bits
			M_RS15 = 15,
			M_RS14 = 14,
			M_RS13 = 13,
			M_RS12 = 12,
			M_RS11 = 11,
			M_RS10 = 10,
			M_RS9 = 9,
			M_RS8 = 8,
			M_RS7 = 7,
			M_RS6 = 6,
			M_RS5 = 5,
			M_RS4 = 4,
			M_RS3 = 3,
			M_RS2 = 2,
			M_RS1 = 1,
			M_RS0 = 0,

			// TSMB Register bits
			M_TS31 = 15,
			M_TS30 = 14,
			M_TS29 = 13,
			M_TS28 = 12,
			M_TS27 = 11,
			M_TS26 = 10,
			M_TS25 = 9,
			M_TS24 = 8,
			M_TS23 = 7,
			M_TS22 = 6,
			M_TS21 = 5,
			M_TS20 = 4,
			M_TS19 = 3,
			M_TS18 = 2,
			M_TS17 = 1,
			M_TS16 = 0,

			// TSMA Register bits
			M_TS15 = 15,
			M_TS14 = 14,
			M_TS13 = 13,
			M_TS12 = 12,
			M_TS11 = 11,
			M_TS10 = 10,
			M_TS9 = 9,
			M_TS8 = 8,
			M_TS7 = 7,
			M_TS6 = 6,
			M_TS5 = 5,
			M_TS4 = 4,
			M_TS3 = 3,
			M_TS2 = 2,
			M_TS1 = 1,
			M_TS0 = 0,

			// RCCR Register bits
			M_RHCKD = 23,
			M_RFSD = 22,
			M_RCKD = 21,
			M_RHCKP = 20,
			M_RFSP = 19,
			M_RCKP = 18,

			M_RFP = 0x3C000,
			M_RFP3 = 17,
			M_RFP2 = 16,
			M_RFP1 = 15,
			M_RFP0 = 14,

			M_RDC = 0x3E00,
			M_RDC4 = 13,
			M_RDC3 = 12,
			M_RDC2 = 11,
			M_RDC1 = 10,
			M_RDC0 = 9,
			M_RPSR = 8,

			M_RPM = 0xFF,
			M_RPM7 = 7,
			M_RPM6 = 6,
			M_RPM5 = 5,
			M_RPM4 = 4,
			M_RPM3 = 3,
			M_RPM2 = 2,
			M_RPM1 = 1,
			M_RPM0 = 0,

			// RCR Register bits
			M_RLIE = 23,
			M_RIE = 22,
			M_REDIE = 21,
			M_REIE = 20,
			M_RPR = 19,
			M_RFSR = 16,
			M_RFSL = 15,

			M_RSWS = 0x7C00,
			M_RSWS4 = 14,
			M_RSWS3 = 13,
			M_RSWS2 = 12,
			M_RSWS1 = 11,
			M_RSWS0 = 10,

			M_RMOD = 0x300,
			M_RMOD1 = 9,
			M_RMOD0 = 8,
			M_RWA = 7,
			M_RSHFD = 6,

			M_RE = 0xF,
			M_RE3 = 3,
			M_RE2 = 2,
			M_RE1 = 1,
			M_RE0 = 0,

			// TCCR Register bits
			M_THCKD = 23,
			M_TFSD = 22,
			M_TCKD = 21,
			M_THCKP = 20,
			M_TFSP = 19,
			M_TCKP = 18,

			M_TFP = 0x3C000,
			M_TFP3 = 17,
			M_TFP2 = 16,
			M_TFP1 = 15,
			M_TFP0 = 14,

			M_TDC = 0x3E00,
			M_TDC4 = 13,
			M_TDC3 = 12,
			M_TDC2 = 11,
			M_TDC1 = 10,
			M_TDC0 = 9,
			M_TPSR = 8,

			M_TPM = 0xFF,
			M_TPM7 = 7,
			M_TPM6 = 6,
			M_TPM5 = 5,
			M_TPM4 = 4,
			M_TPM3 = 3,
			M_TPM2 = 2,
			M_TPM1 = 1,
			M_TPM0 = 0,

			// TCR Register bits
			M_TLIE = 23,
			M_TIE = 22,
			M_TEDIE = 21,
			M_TEIE = 20,
			M_TPR = 19,
			M_PADC = 17,
			M_TFSR = 16,
			M_TFSL = 15,

			M_TSWS = 0x7C00,
			M_TSWS4 = 14,
			M_TSWS3 = 13,
			M_TSWS2 = 12,
			M_TSWS1 = 11,
			M_TSWS0 = 10,

			M_TMOD = 0x300,
			M_TMOD1 = 9,
			M_TMOD0 = 8,
			M_TWA = 7,
			M_TSHFD = 6,

			M_TEM = 0x3F,
			M_TE5 = 5,
			M_TE4 = 4,
			M_TE3 = 3,
			M_TE2 = 2,
			M_TE1 = 1,
			M_TE0 = 0,

			// control bits of SAICR
			M_ALC = 8,
			M_TEBE = 7,
			M_SYN = 6,
			M_OF2 = 2,
			M_OF1 = 1,
			M_OF0 = 0,

			// status bits of SAISR
			M_TODE = 17,
			M_TEDE = 16,
			M_TDE = 15,
			M_TUE = 14,
			M_TFS = 13,
			M_RODF = 10,
			M_REDF = 9,
			M_RDF = 8,
			M_ROE = 7,
			M_RFS = 6,
			M_IF2 = 2,
			M_IF1 = 1,
			M_IF0 = 0,
		};

		explicit Esai(IPeripherals& _periph);

		TWord readStatusRegister()
		{
			return m_sr;
		}

		void exec()
		{
		}

	private:
		IPeripherals& m_periph;
		TWord m_sr = 0;	// status register
	};
}
