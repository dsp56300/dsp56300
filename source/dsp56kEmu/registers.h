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
	enum CCRBit
	{
		SRB_C,					// carry
		SRB_V,					// overflow
		SRB_Z,					// zero
		SRB_N,					// negative
		SRB_U,					// unnormalized
		SRB_E,					// extension
		SRB_L,					// limit
		SRB_S,					// scaling		

		SRB_I0,					// interrupt mask bit 0
		SRB_I1,					// interrupt mask bit 1
		SRB_S0,					// scaling bit 0
		SRB_S1,					// scaling bit 1
	};

	enum CCRMask
	{
		SR_C = (1<<SRB_C),		// carry
		SR_V = (1<<SRB_V),		// overflow
		SR_Z = (1<<SRB_Z),		// zero
		SR_N = (1<<SRB_N),		// negative
		SR_U = (1<<SRB_U),		// unnormalized
		SR_E = (1<<SRB_E),		// extension
		SR_L = (1<<SRB_L),		// limit
		SR_S = (1<<SRB_S),		// scaling

	// bit 8-15
		SR_I0	= 0x000100,	// interrupt mask bit 0
		SR_I1	= 0x000200,	// interrupt mask bit 1
		SR_S0	= 0x000400,	// scaling bit 0
		SR_S1	= 0x000800,	// scaling bit 1
		__SR_12	= 0x001000,	// not used
		SR_SC	= 0x002000,	// sixteen-bit compatibility mode
		SR_DM	= 0x004000,	// double-precision multiply mode
		SR_LF	= 0x008000,	// DO loop flag

	// bits 16-23
		SR_FV	= 0x010000,	// DO forever
		SR_SA	= 0x020000,	// sixteen-bit arithmetic mode
		__SR_18	= 0x040000,	// not used
		SR_CE	= 0x080000,	// cache enable
		SR_SM	= 0x100000,	// arithmetic saturation mode
		SR_RM	= 0x200000,	// rounding mode
		SR_CP0	= 0x400000,	// core priority bit 0
		SR_CP1	= 0x800000,	// core priority bit 1
	};

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

	enum EffectiveAddressingMode
	{
		MMMRRR_RnMinusNn		= 0b000'000,		
		MMMRRR_RnPlusNn			= 0b001'000,		
		MMMRRR_RnMinus			= 0b010'000,		
		MMMRRR_RnPlus			= 0b011'000,		
		MMMRRR_Rn				= 0b100'000,		
		MMMRRR_RnPlusNnUpdate	= 0b101'000,		
		MMMRRR_AbsAddr			= 0b110'000,
		MMMRRR_ImmediateData	= 0b110'100,
		MMMRRR_MinusRn			= 0b111'000,

		MMM_RnMinusNn			= 0b000,	/* 000 (Rn)-Nn */		
		MMM_RnPlusNn			= 0b001,	/* 001 (Rn)+Nn */		
		MMM_RnMinus				= 0b010,	/* 010 (Rn)-   */		
		MMM_RnPlus				= 0b011,	/* 011 (Rn)+   */		
		MMM_Rn					= 0b100,	/* 100 (Rn)    */
		MMM_RnPlusNnUpdate		= 0b101,	/* 101 (Rn+Nn) */		
		MMM_AbsAddr				= 0b110,
		MMM_ImmediateData		= 0b110,
		MMM_MinusRn				= 0b111,	/* 111 -(Rn)   */		

		RRR_ImmediateData		= 0b100,
	};

	enum OnChipRegisterType
	{
		_i_X0,		_i_X1,		_i_Y0,		_i_Y1,
		
		// 0 0 0 1 D D
		DD_X0,		DD_X1,		DD_Y0,		DD_Y1,

		// 0 0 1 D D D
		DDD_A0,		DDD_B0,		DDD_A2,		DDD_B2,		DDD_A1,		DDD_B1,		DDD_A,		DDD_B,

		// 0 1 0 T T T
		TTT_R0,		TTT_R1,		TTT_R2,		TTT_R3,		TTT_R4,		TTT_R5,		TTT_R6,		TTT_R7,

		// 0 1 1 N N N
		NNN_N0,		NNN_N1,		NNN_N2,		NNN_N3,		NNN_N4,		NNN_N5,		NNN_N6,		NNN_N7,

		// 1 0 0 F F F
		FFF_M0,		FFF_M1,		FFF_M2,		FFF_M3,		FFF_M4,		FFF_M5,		FFF_M6,		FFF_M7,

		// 1 0 0 E E E
		_EEE_i0,	_EEE_i1,	EEE_EP,		_EEE_i3,	_EEE_i4,	_EEE_i5,	_EEE_i6,	_EEE_i7,

		// 1 0 0 F F F
		VVV_VBA,	VVV_SC,		_VVV_i2,	_VVV_i3,	_VVV_i4,	_VVV_i5,	_VVV_i6,	_VVV_i7,

		// 1 0 0 F F F
		GGG_SZ,		GGG_SR,		GGG_OMR,	GGG_SP,		GGG_SSH,	GGG_SSL,	GGG_LA,		GGG_LC,
	};
};
