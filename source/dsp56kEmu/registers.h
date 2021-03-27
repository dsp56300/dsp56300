#pragma once

#include "types.h"

namespace dsp56k
{
	enum EReg
	{
		Reg_X, Reg_Y,		// 48bit

		Reg_A, Reg_B,		// 56bit

		Reg_X0,		Reg_X1,		
		Reg_Y0,		Reg_Y1,		

		Reg_A0,		Reg_A1,		Reg_A2,		
		Reg_B0,		Reg_B1,		Reg_B2,		

		Reg_PC,		Reg_SR,		Reg_OMR,	

		Reg_LA,		Reg_LC,		

		Reg_SSH,	Reg_SSL,	Reg_SP,		

		Reg_EP,		Reg_SZ,		Reg_SC,		Reg_VBA,	

		Reg_IPRC,	Reg_IPRP,	Reg_BCR,	Reg_DCR,	

		Reg_AAR0,	Reg_AAR1,	Reg_AAR2,	Reg_AAR3,	

		Reg_R0,	Reg_R1,	Reg_R2,	Reg_R3,	Reg_R4,	Reg_R5,	Reg_R6,	Reg_R7,		
		Reg_N0,	Reg_N1,	Reg_N2,	Reg_N3,	Reg_N4,	Reg_N5,	Reg_N6,	Reg_N7,		
		Reg_M0,	Reg_M1,	Reg_M2,	Reg_M3,	Reg_M4,	Reg_M5,	Reg_M6,	Reg_M7,		

		Reg_HIT, Reg_MISS,
		Reg_REPLACE,	
		Reg_CYC,		
		Reg_ICTR,		
		Reg_CNT1, Reg_CNT2, Reg_CNT3, Reg_CNT4,		

		Reg_COUNT
	};

	// doc\plugin-DSP56303UM.pdf

	// SR - Status register

	// CCR - bit 0-7
	#define			SRB_C			0			// carry
	#define 		SRB_V			1			// overflow
	#define 		SRB_Z			2			// zero
	#define 		SRB_N			3			// negative
	#define 		SRB_U			4			// unnormalized
	#define 		SRB_E			5			// extension
	#define 		SRB_L			6			// limit
	#define 		SRB_S			7			// scaling

	#define			SR_C			(1<<SRB_C)	// carry
	#define 		SR_V			(1<<SRB_V)	// overflow
	#define 		SR_Z			(1<<SRB_Z)	// zero
	#define 		SR_N			(1<<SRB_N)	// negative
	#define 		SR_U			(1<<SRB_U)	// unnormalized
	#define 		SR_E			(1<<SRB_E)	// extension
	#define 		SR_L			(1<<SRB_L)	// limit
	#define 		SR_S			(1<<SRB_S)	// scaling

	// bit 8-15
	#define 		SR_I0			0x000100	// interrupt mask bit 0
	#define 		SR_I1			0x000200	// interrupt mask bit 1
	#define 		SR_S0			0x000400	// scaling bit 0
	#define 		SR_S1			0x000800	// scaling bit 1
	#define 		__SR_12			0x001000	// not used
	#define 		SR_SC			0x002000	// sixteen-bit compatibility mode
	#define 		SR_DM			0x004000	// double-precision multiply mode
	#define 		SR_LF			0x008000	// DO loop flag

	// bits 16-23
	#define 		SR_FV			0x010000	// DO forever
	#define 		SR_SA			0x020000	// sixteen-bit arithmetic mode
	#define 		__SR_18			0x040000	// not used
	#define 		SR_CE			0x080000	// cache enable
	#define 		SR_SM			0x100000	// arithmetic saturation mode
	#define 		SR_RM			0x200000	// rounding mode
	#define 		SR_CP0			0x400000	// core priority bit 0
	#define 		SR_CP1			0x800000	// core priority bit 1

	// OMR - Operating Mode Register
	#define			OMR_MA			0x000001	// Chip Operating Mode
	#define			OMR_MB			0x000002	// Chip Operating Mode
	#define			OMR_MC			0x000004	// Chip Operating Mode
	#define			OMR_MD			0x000008	// Chip Operating Mode
	#define			OMR_EBD			0x000010	// External Bus Disable
	#define			OMR_ResvdBit5	0x000020	// Reserved, write to zero for future compatibility
	#define			OMR_SD			0x000040	// Stop Delay Mode
	#define			OMR_MS			0x000080	// Memory Switch Mode

	#define			OMR_CDP0		0x000100	// Core DMA Priority
	#define			OMR_CDP1		0x000200	// Core DMA Priority
	#define			OMR_BE			0x000400	// Cache Burst Mode Enable
	#define			OMR_TAS			0x000800	// TA Synchronize Select
	#define			OMR_BRT			0x001000	// Bus Release Timing
	#define			OMR_ABE			0x002000	// Asynchronous Bus Arbitration Enable
	#define			OMR_APD			0x004000	// Address Attribute Priority Disable
	#define			OMR_ATE			0x008000	// Address Trace Enable

	#define			OMR_XYS			0x010000	// Stack Extension XY Select
	#define			OMR_EUN			0x020000	// Stack Extension Underflow Flag
	#define			OMR_EOV			0x040000	// Stack Extension Overflow Flag
	#define			OMR_WRP			0x080000	// Stack Extension Wrap Flag
	#define			OMR_SEN			0x100000	// Stack Extension Enable
	#define			OMR_MSW0		0x200000	// Memory Switch Configuration
	#define			OMR_MSW1		0x400000	// Memory Switch Configuration
	#define			OMR_PEN			0x800000	// Patch Enable

	struct SRegs
	{
		TReg48	x,y;		// 48 bit

		TReg56	a,b;		// 56 bit

		TReg24	x0, x1, y0, y1;

		TReg24	a0, a1;
		TReg8	a2;

		TReg24	b0, b1;
		TReg8	b2;

		TReg24 pc, sr, omr;

		TReg24 la, lc;

		TReg24 ssh, ssl, sp;	

		TReg24 ep, sz, sc, vba;

		TReg24 iprc, iprp, bcr, dcr;

		TReg24 aar0, aar1, aar2, aar3;

		TReg24 r0, r1, r2, r3, r4, r5, r6, r7;
		TReg24 n0, n1, n2, n3, n4, n5, n6, n7;
		TReg24 m0, m1, m2, m3, m4, m5, m6, m7;

		TReg24 hit;		
		TReg24 miss;		
		TReg24 replace;	
		TReg24 cyc;		
		TReg24 ictr;		
		TReg24 cnt1;		
		TReg24 cnt2;		
		TReg24 cnt3;		
		TReg24 cnt4;
	};

	extern const size_t g_regBitCount[Reg_COUNT];
	extern const char* const g_regNames[Reg_COUNT];

	enum ConditionCode
	{
		CCCC_CarrySet		= 0x8,
		CCCC_CarryClear		= 0x0,
		CCCC_ExtensionSet	= 0xd,
		CCCC_ExtensionClear	= 0x5,
		CCCC_Equal			= 0xa,
		CCCC_NotEqual		= 0x2,
		CCCC_LimitSet		= 0xe,
		CCCC_LimitClear		= 0x6,
		CCCC_Minus			= 0xb,
		CCCC_Plus			= 0x3,
		CCCC_GreaterEqual	= 0x1,
		CCCC_LessThan		= 0x9,
		CCCC_Normalized		= 0xc,
		CCCC_NotNormalized	= 0x4,
		CCCC_GreaterThan	= 0x7,
		CCCC_LessEqual		= 0xf
	};
};
