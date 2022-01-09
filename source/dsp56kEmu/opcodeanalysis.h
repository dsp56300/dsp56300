#pragma once

#include <cstdint>

#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

namespace dsp56k
{
	enum class Register : uint64_t
	{
		Invalid,

		// individual registers

		A0, A1, A2,
		B0, B1, B2,

		X0, X1,
		Y0, Y1,

		R0, R1, R2, R3, R4, R5, R6, R7,
		N0, N1, N2, N3, N4, N5, N6, N7,
		M0, M1, M2, M3, M4, M5, M6, M7,

		PC, LA, LC, SSH, SSL, SP, SC, EP, VBA, SZ, MR, CCR, EOM, COM,

		// combinations

		A, B,
		A10, B10, AB, BA,

		X, Y,
		X0X0, X0X1, X1X0, X1X1,
		Y0Y0, Y0Y1, Y1Y0, Y1Y1,
		X0Y0, X0Y1, X1Y0, X1Y1,
		Y0X0, Y0X1, Y1X0, Y1X1,

		SR, OMR,
	};

	static_assert(static_cast<uint32_t>(Register::COM) < 64);

	enum class RegisterMask : uint64_t
	{
		A0 = (1ull << static_cast<uint64_t>(Register::A0)),
		A1 = (1ull << static_cast<uint64_t>(Register::A1)),
		A2 = (1ull << static_cast<uint64_t>(Register::A2)),
		B0 = (1ull << static_cast<uint64_t>(Register::B0)),
		B1 = (1ull << static_cast<uint64_t>(Register::B1)),
		B2 = (1ull << static_cast<uint64_t>(Register::B2)),

		A = A0 | A1 | A2,
		B = B0 | B1 | B2,

		AB = A | B,
		A10 = A1 | A0,
		BA = B | A,
		B10 = B1 | B0,

		X0 = (1ull << static_cast<uint64_t>(Register::X0)),
		X1 = (1ull << static_cast<uint64_t>(Register::X1)),
		Y0 = (1ull << static_cast<uint64_t>(Register::Y0)),
		Y1 = (1ull << static_cast<uint64_t>(Register::Y1)),

		X = X0 | X1,
		Y = Y0 | Y1,

		X0Y0 = X0 | Y0,
		X0Y1 = X0 | Y1,
		X1Y0 = X1 | Y0,
		X1Y1 = X1 | Y1,
		Y0X0 = Y0 | X0,
		Y0X1 = Y0 | X1,
		Y1X0 = Y1 | X0,
		Y1X1 = Y1 | X1,

		R0 = (1ull << static_cast<uint64_t>(Register::R0)),
		R1 = (1ull << static_cast<uint64_t>(Register::R1)),
		R2 = (1ull << static_cast<uint64_t>(Register::R2)),
		R3 = (1ull << static_cast<uint64_t>(Register::R3)),
		R4 = (1ull << static_cast<uint64_t>(Register::R4)),
		R5 = (1ull << static_cast<uint64_t>(Register::R5)),
		R6 = (1ull << static_cast<uint64_t>(Register::R6)),
		R7 = (1ull << static_cast<uint64_t>(Register::R7)),

		N0 = (1ull << static_cast<uint64_t>(Register::N0)),
		N1 = (1ull << static_cast<uint64_t>(Register::N1)),
		N2 = (1ull << static_cast<uint64_t>(Register::N2)),
		N3 = (1ull << static_cast<uint64_t>(Register::N3)),
		N4 = (1ull << static_cast<uint64_t>(Register::N4)),
		N5 = (1ull << static_cast<uint64_t>(Register::N5)),
		N6 = (1ull << static_cast<uint64_t>(Register::N6)),
		N7 = (1ull << static_cast<uint64_t>(Register::N7)),

		M0 = (1ull << static_cast<uint64_t>(Register::M0)),
		M1 = (1ull << static_cast<uint64_t>(Register::M1)),
		M2 = (1ull << static_cast<uint64_t>(Register::M2)),
		M3 = (1ull << static_cast<uint64_t>(Register::M3)),
		M4 = (1ull << static_cast<uint64_t>(Register::M4)),
		M5 = (1ull << static_cast<uint64_t>(Register::M5)),
		M6 = (1ull << static_cast<uint64_t>(Register::M6)),
		M7 = (1ull << static_cast<uint64_t>(Register::M7)),

		PC = (1ull << static_cast<uint64_t>(Register::PC)),
		LA = (1ull << static_cast<uint64_t>(Register::LA)),
		LC = (1ull << static_cast<uint64_t>(Register::LC)),
		SSH = (1ull << static_cast<uint64_t>(Register::SSH)),
		SSL = (1ull << static_cast<uint64_t>(Register::SSL)),
		SP = (1ull << static_cast<uint64_t>(Register::SP)),
		SC = (1ull << static_cast<uint64_t>(Register::SC)),
		EP = (1ull << static_cast<uint64_t>(Register::EP)),
		SZ = (1ull << static_cast<uint64_t>(Register::SZ)),
		MR = (1ull << static_cast<uint64_t>(Register::MR)),
		CCR = (1ull << static_cast<uint64_t>(Register::CCR)),
		EOM = (1ull << static_cast<uint64_t>(Register::EOM)),
		COM = (1ull << static_cast<uint64_t>(Register::COM)),

		SR = MR | CCR,
		OMR = EOM | COM,
	};

	constexpr RegisterMask convert(const Register _reg)
	{
		switch (_reg)
		{
		case Register::A:		return RegisterMask::A;
		case Register::B:		return RegisterMask::B;

		case Register::AB:		return RegisterMask::AB;
		case Register::A10:		return RegisterMask::A10;
		case Register::BA:		return RegisterMask::BA;
		case Register::B10:		return RegisterMask::B10;

		case Register::X:		return RegisterMask::X;
		case Register::Y:		return RegisterMask::Y;
		case Register::X0:		return RegisterMask::X0;
		case Register::X1:		return RegisterMask::X1;
		case Register::Y0:		return RegisterMask::Y0;
		case Register::Y1:		return RegisterMask::Y1;
		case Register::X0Y0:	return RegisterMask::X0Y0;
		case Register::X0Y1:	return RegisterMask::X0Y1;
		case Register::X1Y0:	return RegisterMask::X1Y0;
		case Register::X1Y1:	return RegisterMask::X1Y1;
		case Register::Y0X0:	return RegisterMask::Y0X0;
		case Register::Y0X1:	return RegisterMask::Y0X1;
		case Register::Y1X0:	return RegisterMask::Y1X0;
		case Register::Y1X1:	return RegisterMask::Y1X1;

		case Register::SR:		return RegisterMask::SR;
		case Register::OMR:		return RegisterMask::OMR;
		default:
			assert(_reg < 64);
			return static_cast<RegisterMask>(1ull << static_cast<uint64_t>(_reg));
		}
	}

	constexpr Register getRegisterDDDDDD(const TWord _dddddd)
	{
		switch (_dddddd)
		{
			// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
			case 0x00:	return Register::X0;
			case 0x01:	return Register::X1;
			case 0x02:	return Register::Y0;
			case 0x03:	return Register::Y1;
			// 0001DD - 4 registers in data ALU
			case 0x04:	return Register::X0;
			case 0x05:	return Register::X1;
			case 0x06:	return Register::Y0;
			case 0x07:	return Register::Y1;

			// 001DDD - 8 accumulators in data ALU
			case 0x08:	return Register::A0;
			case 0x09:	return Register::B0;
			case 0x0a:	return Register::A2;
			case 0x0b:	return Register::B2;
			case 0x0c:	return Register::A1;
			case 0x0d:	return Register::B1;
			case 0x0e:	return Register::A;
			case 0x0f:	return Register::B;

			// 010TTT - 8 address registers in AGU
			case 0x10:	return Register::R0;
			case 0x11:	return Register::R1;
			case 0x12:	return Register::R2;
			case 0x13:	return Register::R3;
			case 0x14:	return Register::R4;
			case 0x15:	return Register::R5;
			case 0x16:	return Register::R6;
			case 0x17:	return Register::R7;

			// 011NNN - 8 address offset registers in AGU
			case 0x18:	return Register::N0;
			case 0x19:	return Register::N1;
			case 0x1a:	return Register::N2;
			case 0x1b:	return Register::N3;
			case 0x1c:	return Register::N4;
			case 0x1d:	return Register::N5;
			case 0x1e:	return Register::N6;
			case 0x1f:	return Register::N7;

			// 100FFF - 8 address modifier registers in AGU
			case 0x20:	return Register::M0;
			case 0x21:	return Register::M1;
			case 0x22:	return Register::M2;
			case 0x23:	return Register::M3;
			case 0x24:	return Register::M4;
			case 0x25:	return Register::M5;
			case 0x26:	return Register::M6;
			case 0x27:	return Register::M7;

			// 101EEE - 1 adress register in AGU
			case 0x2a:	return Register::EP;

			// 110VVV - 2 program controller registers
			case 0x30:	return Register::VBA;
			case 0x31:	return Register::SC;

			// 111GGG - 8 program controller registers
			case 0x38:	return Register::SZ;
			case 0x39:	return Register::SR;
			case 0x3a:	return Register::OMR;
			case 0x3b:	return Register::SP;
			case 0x3c:	return Register::SSH;
			case 0x3d:	return Register::SSL;
			case 0x3e:	return Register::LA;
			case 0x3f:	return Register::LC;
			default:
				return Register::Invalid;
		}
	}

	constexpr Register getRegister_DDDD(const TWord _dddd)
	{
		return static_cast<Register>(static_cast<uint32_t>(_dddd < 8 ? Register::R0 : Register::N0) + (_dddd & 7));
	}
	constexpr Register getRegister_ddddd_pcr(TWord _ddddd)
	{
		if ((_ddddd & 0x18) == 0x00)
		{
			return static_cast<Register>(static_cast<uint32_t>(Register::M0) + (_ddddd & 0x07));
		}

		switch (_ddddd)
		{
		case 0xa:	return Register::EP;
		case 0x10:	return Register::VBA;
		case 0x11:	return Register::SC;
		case 0x18:	return Register::SZ;
		case 0x19:	return Register::SR;
		case 0x1a:	return Register::OMR;
		case 0x1b:	return Register::SP;
		case 0x1c:	return Register::SSH;
		case 0x1d:	return Register::SSL;
		case 0x1e:	return Register::LA;
		case 0x1f:	return Register::LC;
		default:
			return Register::Invalid;
		}
	}
	constexpr Register getRegister_ee(const TWord _ee)
	{
		switch (_ee)
		{
		case 0:	return Register::X0;
		case 1:	return Register::X1;
		case 2:	return Register::A;
		case 3:	return Register::B;
		default:
			return Register::Invalid;
		}
	}
	constexpr Register getRegister_EE(TWord _ee)
	{
		switch (_ee)
		{
		case 0:	return Register::MR;
		case 1:	return Register::CCR;
		case 2:	return Register::COM;
		case 3:	return Register::EOM;
		default:
			return Register::Invalid;
		}
	}
	constexpr Register getRegister_ff(TWord _ff)
	{
		switch (_ff)
		{
		case 0:	return Register::Y0;
		case 1:	return Register::Y1;
		case 2:	return Register::A;
		case 3:	return Register::B;
		default:
			return Register::Invalid;
		}
	}
	constexpr Register getRegister_GGG(const TWord _ggg, const bool D)
	{
		switch (_ggg)
		{
		case 0:		return D ? Register::A : Register::B;
		case 4:		return Register::X0;
		case 5:		return Register::Y0;
		case 6:		return Register::X1;
		case 7:		return Register::Y1;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_JJ(TWord jj)
	{
		switch (jj)
		{
		case 0: return Register::X0;
		case 1: return Register::Y0;
		case 2: return Register::X1;
		case 3: return Register::Y1;
		default:return Register::Invalid;
		}
	}
	constexpr Register getRegister_JJJ(const TWord JJJ, const bool ab)
	{
		switch (JJJ)
		{
		case 0://	return Register::Invalid;
		case 1:		return ab ? Register::A : Register::B;	// ab = D, if D is a, the result is b and if D is b, the result is a
		case 2:		return Register::X;
		case 3:		return Register::Y;
		case 4:		return Register::X0;
		case 5:		return Register::Y0;
		case 6:		return Register::X1;
		case 7:		return Register::Y1;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_LLL(TWord _lll)
	{
		switch (_lll)
		{
		case 0:		return Register::A10;
		case 1:		return Register::B10;
		case 2:		return Register::X;
		case 3:		return Register::Y;
		case 4:		return Register::A;
		case 5:		return Register::B;
		case 6:		return Register::AB;
		case 7:		return Register::BA;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_qq(TWord _qq)
	{
		switch (_qq)
		{
		case 0:		return Register::X0;
		case 1:		return Register::Y0;
		case 2:		return Register::X1;
		case 3:		return Register::Y1;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_QQ(TWord _qq)
	{
		switch (_qq)
		{
		case 0:		return Register::Y1;
		case 1:		return Register::X0;
		case 2:		return Register::Y0;
		case 3:		return Register::X1;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_RRR(TWord _rrr)
	{
		switch (_rrr)
		{
		case 0:		return Register::R0;
		case 1:		return Register::R1;
		case 2:		return Register::R2;
		case 3:		return Register::R3;
		case 4:		return Register::R4;
		case 5:		return Register::R5;
		case 6:		return Register::R6;
		case 7:		return Register::R7;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_qqq(TWord _qqq)
	{
		switch (_qqq)
		{
		case 2:		return Register::A0;
		case 3:		return Register::B0;
		case 4:		return Register::X0;
		case 5:		return Register::Y0;
		case 6:		return Register::X1;
		case 7:		return Register::Y1;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_QQQQ(const TWord _qqqq)
	{
		switch (_qqqq)
		{
		case 0:		return Register::X0X0;
		case 1:		return Register::Y0Y0;
		case 2:		return Register::X1X0;
		case 3:		return Register::Y1Y0;
		case 4:		return Register::X0Y1;
		case 5:		return Register::Y0X0;
		case 6:		return Register::X1Y0;
		case 7:		return Register::Y1X1;
		case 8:		return Register::X1X1;
		case 9:		return Register::Y1Y1;
		case 10:	return Register::X0X1;
		case 11:	return Register::Y0Y1;
		case 12:	return Register::Y1X0;
		case 13:	return Register::X0Y0;
		case 14:	return Register::Y0X1;
		case 15:	return Register::X1Y1;
		default:	return Register::Invalid;
		}
	}
	constexpr Register getRegister_sss(const TWord _sss)
	{
		switch (_sss)
		{
		case 2:		return Register::A1;
		case 3:		return Register::B1;
		case 4:		return Register::X0;
		case 5:		return Register::Y0;
		case 6:		return Register::X1;
		case 7:		return Register::Y1;
		default:	return Register::Invalid;
		}
	}

	constexpr Register getRegister(Instruction _inst, Field _field, TWord op)
	{
		switch (_field)
		{
		case Field_CCCC: 
			return Register::CCR;
		case Field_d:
		case Field_D:
			return getFieldValue(_inst, _field, op) ? Register::B : Register::A;
		case Field_dd:
		case Field_ddd:
			return getRegisterDDDDDD(getFieldValue(_inst, Field_dd, Field_ddd, op));
		case Field_dddd:
			return getRegister_DDDD(getFieldValue(_inst, _field, op));
		case Field_ddddd:
		case Field_dddddd:
		case Field_DDDD:
		case Field_DDDDDD:
		case Field_eeeee:
		case Field_eeeeee:
			return getRegisterDDDDDD(getFieldValue(_inst, _field, op));
		case Field_DDDDD:
			return getRegister_ddddd_pcr(getFieldValue(_inst, _field, op));
		case Field_e: 
			return getFieldValue(_inst, _field, op) ? Register::X1 : Register::X0;
		case Field_ee:
			return getRegister_ee(getFieldValue(_inst, _field, op));
		case Field_EE:
			return getRegister_EE(getFieldValue(_inst, _field, op));
		case Field_ff:
			switch (_inst)
			{
			case Movexr_ea:	return getRegister_ee(getFieldValue(_inst, _field, op));
			case Moveyr_ea:
			case Movexy:	return getRegister_ff(getFieldValue(_inst, _field, op));
			default:		return Register::Invalid;
			}
		case Field_F: 
			return getFieldValue(_inst, _field, op) ? Register::Y1 : Register::Y0;
		case Field_ggg:
			return getRegister_GGG(getFieldValue(_inst, _field, op), getFieldValue(_inst, Field_d, op));
		case Field_J:
			return getFieldValue(_inst, _field, op) ? Register::Y : Register::X;
		case Field_JJ:
			return getRegister_JJ(getFieldValue(_inst, _field, op));
		case Field_JJJ:
			return getRegister_JJJ(getFieldValue(_inst, _field, op), getFieldValue(_inst, Field_d, op));
		case Field_L:
		case Field_LL:
			return getRegister_LLL(getFieldValue(_inst, Field_L, Field_LL, op));
		case Field_qq:
			return getRegister_qq(getFieldValue(_inst, _field, op));
		case Field_qqq:
			return getRegister_qqq(getFieldValue(_inst, _field, op));
		case Field_QQ:
			return getRegister_QQ(getFieldValue(_inst, _field, op));
		case Field_QQQ: 
		case Field_QQQQ:
			return getRegister_QQQQ(getFieldValue(_inst, _field, op));
		case Field_rr: 
			{
				const TWord mrX = getFieldValue(_inst, Field_MM, Field_RRR, op);
				TWord rrY = getFieldValue(_inst, Field_rr, op);
				rrY += ((mrX & 0x7) >= 4) ? 0 : 4;
				return getRegister_RRR(rrY);
			}
		case Field_RRR: 
			return getRegister_RRR(getFieldValue(_inst, Field_L, Field_LL, op));
		case Field_s:
			switch (_inst)
			{
			case Extract_CoS2:
			case Extract_S1S2:
			case Extractu_CoS2:
			case Extractu_S1S2:		return getFieldValue(_inst, _field, op) ? Register::B : Register::A;
			default:				return Register::Invalid;
			}
		case Field_sss: 
			return getRegister_sss(getFieldValue(_inst, _field, op));
		case Field_S: 
			switch (_inst)
			{
			case Asl_ii:
			case Asr_ii:
			case Asl_S1S2D:
			case Asr_S1S2D:
			case Clb:
			case Vsl:				return getFieldValue(_inst, _field, op) ? Register::B : Register::A;
			default:				return Register::Invalid;
			}
		case Field_SSS:
			return getRegister_sss(getFieldValue(_inst, _field, op));
		case Field_ttt:
		case Field_TTT:
			return getRegister_RRR(getFieldValue(_inst, _field, op));
		default: 
			return Register::Invalid;
		}
	}
}
