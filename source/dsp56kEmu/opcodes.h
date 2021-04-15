#pragma once

#include <vector>

#include "opcodefields.h"
#include "opcodeinfo.h"
#include "opcodetypes.h"
#include "types.h"

namespace dsp56k
{
	const FieldInfo& getFieldInfo(Instruction _i, Field _f);

	template<Instruction I, Field F>
	constexpr FieldInfo static getFieldInfoCE()
	{
		static_assert(initField(g_opcodes[I].m_opcode, g_fieldParseConfigs[F].ch, g_fieldParseConfigs[F].count).len > 0, "field not known");
		return initField(g_opcodes[I].m_opcode, g_fieldParseConfigs[F].ch, g_fieldParseConfigs[F].count);
	}

	static constexpr TWord getFieldValue(const FieldInfo& _fi, TWord _memoryValue)
	{
		assert(_fi.len > 0 && "field not known");
		return (_memoryValue >> _fi.bit) & _fi.mask;
	}

	template<Instruction I, Field F> static TWord getFieldValue(const TWord _memValue)
	{
		constexpr auto fi = getFieldInfoCE<I,F>();
		return getFieldValue(fi, _memValue);
	}

	template<Instruction I, Field MSB, Field LSB> static TWord getFieldValue(const TWord _memValue)
	{
		constexpr auto fa = getFieldInfoCE<I,MSB>();
		constexpr auto fb = getFieldInfoCE<I,LSB>();
		return getFieldValue<I,MSB>(_memValue) << fb.len | getFieldValue<I,LSB>(_memValue);
	}

	template<Instruction I> EMemArea getFieldValueMemArea(const TWord _op)
	{
		return static_cast<EMemArea>(getFieldValue<I,Field_S>(_op));
	}
	
	static TWord getFieldValue(Instruction _instruction, const Field _field, TWord _memoryValue)
	{
		const auto& fi = getFieldInfo(_instruction, _field);
		return getFieldValue(fi, _memoryValue);
	}

	static TWord getFieldValue(Instruction _instruction, const Field _msb, const Field _lsb, TWord _memoryValue)
	{
		const auto& fa = getFieldInfo(_instruction, _msb);
		const auto& fb = getFieldInfo(_instruction, _lsb);

		return getFieldValue(fa, _memoryValue) << fb.len | getFieldValue(fb, _memoryValue);
	}

	static bool matchFieldRange(Instruction _instruction, Field _field, TWord _mem, TWord min, TWord max)
	{
		const auto& fi = getFieldInfo(_instruction, _field);
		if(!fi.len)
			return true;

		const auto v = getFieldValue(fi, _mem);
		return v >= min && v <= max;
	}

	static bool matchdddddd(TWord v)
	{
		if(v < 4)
			return false;
		if(v >= 0b101000 && v <= 0b101111 && v != 0b101010)	// For 101EEE, only 010 is valid for EEE (EP register)
			return false;
		if(v >= 0b110010 && v <= 0b110111)					// For 110VVV, only VBA (000) and SC (001) exist
			return false;
		return true;			
	}

	static bool matchdddddd(Instruction _instruction, Field _field, TWord _word)
	{
		return matchdddddd(getFieldValue(_instruction, _field, _word));
	}

	static bool hasField(const OpcodeInfo& _oi, const Field _field)
	{
		return getFieldInfo(_oi.m_instruction, _field).len > 0;
	}

	static bool isNonParallelOpcode(const OpcodeInfo& _oi)
	{
		return !hasField(_oi, Field_MoveOperation) && !hasField(_oi, Field_AluOperation);
	}

	static bool match(const OpcodeInfo& _oi, const TWord _word)
	{
		const auto m = _oi.m_mask1 | _oi.m_mask0;
		if((_word & m) != _oi.m_mask1)
			return false;

		if(!matchFieldRange(_oi.m_instruction, Field_bbbbb, _word, 0, 23))
			return false;

		if(hasField(_oi, Field_JJJ))
		{
			const auto v = getFieldValue(_oi.m_instruction, Field_JJJ, _word);

			if(!OpcodeInfo::isParallelOpcode(_word))
			{
				// There are two variants for JJJ, one does not allow 001, 010 and 011 (56 bit regs), only 24 bit are allowed. For Non-parallel instructions, the only instruction using it is Tcc and there it does not allow all values
				if(v > 0 && v < 4)
					return false;
			}
			else
			{
				switch (_oi.m_instruction)
				{
				case Tfr:
				case Cmp_S1S2:
				case Cmpm_S1S2:
					// There are two variants for JJJ, one does not allow 001, 010 and 011 (56 bit regs), only 24 bit are allowed.
					if(v > 0 && v < 4)
						return false;
				}
			}
		}

		if(hasField(_oi, Field_DDDD))
		{
			const auto v = getFieldValue(_oi.m_instruction, Field_DDDD, _word);
			if(v < 4)
				return false;
		}

		if(hasField(_oi, Field_dddddd))
		{
			if(!matchdddddd(_oi.m_instruction, Field_dddddd, _word))
				return false;
		}

		if(hasField(_oi, Field_ddddd))
		{
			if(!matchdddddd(_oi.m_instruction, Field_ddddd, _word))
				return false;
		}
		
		if(hasField(_oi, Field_dd) && hasField(_oi, Field_ddd))
		{
			if(!matchdddddd(getFieldValue(_oi.m_instruction, Field_dd, Field_ddd, _word)))
				return false;
		}
		
		if(hasField(_oi, Field_eeeee))
		{
			if(!matchdddddd(_oi.m_instruction, Field_eeeee, _word))
				return false;
		}
		
		if(hasField(_oi, Field_eeeeee))
		{
			if(!matchdddddd(_oi.m_instruction, Field_eeeeee, _word))
				return false;
		}
		
		if(hasField(_oi, Field_DDDDD))
		{
			const auto v = getFieldValue(_oi.m_instruction, Field_DDDDD, _word);
			switch(v)
			{
			case 0x0:		// M0
			case 0x1:		// M1
			case 0x2:		// M2
			case 0x3:		// M3
			case 0x4:		// M4
			case 0x5:		// M5
			case 0x6:		// M6
			case 0x7:		// M7
			case 0b01010:	// EP
			case 0b10000:	// VBA
			case 0b10001:	// SC
			case 0b11000:	// SZ
			case 0b11001:	// SR
			case 0b11010:	// OMR
			case 0b11011:	// SP
			case 0b11100:	// SSH
			case 0b11101:	// SSL
			case 0b11110:	// LA
			case 0b11111:	// LC
				break;
			default:
				return false;
			}
		}

		if(!hasField(_oi, Field_MMM))
			return true;

		const auto mmm = getFieldValue(_oi.m_instruction, Field_MMM, _word);
		if(mmm != 0x6)
			return true;

		const auto rrr = getFieldValue(_oi.m_instruction, Field_RRR, _word);
		if(rrr == 0 && (_oi.m_extensionWordType & EffectiveAddress) != 0)
			return true;
		if(rrr == 4 && (_oi.m_extensionWordType & ImmediateData) != 0)
		{
			// If peripheral is target, the source can be an immediate extension word, but if peripheral is source, we cannot write to immediate data
			if(!hasField(_oi, Field_W) || getFieldValue(_oi.m_instruction, Field_W, _word) == 1)
				return true;				
		}
		return false;
	}

	class Opcodes
	{
	public:
		Opcodes();

		const OpcodeInfo* findNonParallelOpcodeInfo(TWord _opcode) const;
		const OpcodeInfo* findParallelMoveOpcodeInfo(TWord _opcode) const;
		const OpcodeInfo* findParallelAluOpcodeInfo(TWord _opcode) const;

		static bool isParallelOpcode(const TWord _opcode)
		{
			return OpcodeInfo::isParallelOpcode(_opcode);
		}

		static bool isNonParallelOpcode(const TWord _opcode)
		{
			return !isParallelOpcode(_opcode);
		}

		static const OpcodeInfo& getOpcodeInfoAt(size_t _index);

	private:
		static const OpcodeInfo* findOpcodeInfo(TWord _opcode, const std::vector<const OpcodeInfo*>& _opcodes);
		
		std::vector<const OpcodeInfo*> m_opcodesNonParallel;
		std::vector<const OpcodeInfo*> m_opcodesMove;
		std::vector<const OpcodeInfo*> m_opcodesAlu;

	};
}
