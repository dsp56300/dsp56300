#pragma once

#include <cstdint>

namespace dsp56k
{
	enum PoolReg
	{
		DspRegInvalid = -1,

		DspR0, DspR1, DspR2, DspR3, DspR4, DspR5, DspR6, DspR7,
		DspN0, DspN1, DspN2, DspN3, DspN4, DspN5, DspN6, DspN7,
		DspM0, DspM1, DspM2, DspM3, DspM4, DspM5, DspM6, DspM7,

		DspA, DspB,
		DspAwrite, DspBwrite,

		DspX, DspY,

		DspX0, DspX1, DspY0, DspY1,

		DspPC,
		DspSR,
		DspLC,
		DspLA,

		TempA, TempB, TempC, TempD, TempE, TempF, TempG, TempH, LastTemp = TempH,

		DspM0mod, DspM1mod, DspM2mod, DspM3mod, DspM4mod, DspM5mod, DspM6mod, DspM7mod,
		DspM0mask, DspM1mask, DspM2mask, DspM3mask, DspM4mask, DspM5mask, DspM6mask, DspM7mask,

		DspCount
	};

	static constexpr uint64_t mask(PoolReg _reg)
	{
		return 1ull << static_cast<uint64_t>(_reg);
	}

	static constexpr uint32_t toIndex(PoolReg _reg)
	{
		return static_cast<uint32_t>(_reg);
	}

	enum class DspRegFlags : uint64_t
	{
		None = 0,

		R0 = mask(PoolReg::DspR0), R1 = mask(PoolReg::DspR1), R2 = mask(PoolReg::DspR2), R3 = mask(PoolReg::DspR3), R4 = mask(PoolReg::DspR4), R5 = mask(PoolReg::DspR5), R6 = mask(PoolReg::DspR6), R7 = mask(PoolReg::DspR7),
		N0 = mask(PoolReg::DspN0), N1 = mask(PoolReg::DspN1), N2 = mask(PoolReg::DspN2), N3 = mask(PoolReg::DspN3), N4 = mask(PoolReg::DspN4), N5 = mask(PoolReg::DspN5), N6 = mask(PoolReg::DspN6), N7 = mask(PoolReg::DspN7),
		M0 = mask(PoolReg::DspM0), M1 = mask(PoolReg::DspM1), M2 = mask(PoolReg::DspM2), M3 = mask(PoolReg::DspM3), M4 = mask(PoolReg::DspM4), M5 = mask(PoolReg::DspM5), M6 = mask(PoolReg::DspM6), M7 = mask(PoolReg::DspM7),

		A = mask(PoolReg::DspA), B = mask(PoolReg::DspB),
		Awrite = mask(PoolReg::DspAwrite), Bwrite = mask(PoolReg::DspBwrite),

		X = mask(PoolReg::DspX), Y = mask(PoolReg::DspY),
		X0 = mask(PoolReg::DspX0), X1 = mask(PoolReg::DspX1),
		Y0 = mask(PoolReg::DspY0), Y1 = mask(PoolReg::DspY1),

		PC = mask(PoolReg::DspPC),
		SR = mask(PoolReg::DspSR),
		LC = mask(PoolReg::DspLC),
		LA = mask(PoolReg::DspLA),

		T0 = mask(PoolReg::TempA), T1 = mask(PoolReg::TempB), T2 = mask(PoolReg::TempC), T3 = mask(PoolReg::TempD), T4 = mask(PoolReg::TempE), T5 = mask(PoolReg::TempF), T6 = mask(PoolReg::TempG), T7 = mask(PoolReg::TempH),

		M0mod = mask(PoolReg::DspM0mod), M1mod = mask(PoolReg::DspM1mod), M2mod = mask(PoolReg::DspM2mod), M3mod = mask(PoolReg::DspM3mod), M4mod = mask(PoolReg::DspM4mod), M5mod = mask(PoolReg::DspM5mod), M6mod = mask(PoolReg::DspM6mod), M7mod = mask(PoolReg::DspM7mod),
		M0mask = mask(PoolReg::DspM0mask), M1mask = mask(PoolReg::DspM1mask), M2mask = mask(PoolReg::DspM2mask), M3mask = mask(PoolReg::DspM3mask), M4mask = mask(PoolReg::DspM4mask), M5mask = mask(PoolReg::DspM5mask), M6mask = mask(PoolReg::DspM6mask), M7mask = mask(PoolReg::DspM7mask),
	};

	static constexpr DspRegFlags operator | (const DspRegFlags& _a, const DspRegFlags& _b)
	{
		return static_cast<DspRegFlags>(static_cast<uint64_t>(_a) | static_cast<uint64_t>(_b));
	}

	static constexpr DspRegFlags operator & (const DspRegFlags& _a, const DspRegFlags& _b)
	{
		return static_cast<DspRegFlags>(static_cast<uint64_t>(_a) & static_cast<uint64_t>(_b));
	}

	static constexpr DspRegFlags operator ~ (const DspRegFlags& _a)
	{
		return static_cast<DspRegFlags>(~static_cast<uint64_t>(_a));
	}
}
