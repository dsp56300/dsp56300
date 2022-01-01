#include "opcodes.h"

#include <array>

namespace dsp56k
{
	struct RuntimeFieldInfo
	{
		std::array<FieldInfo, Field_COUNT> fieldInfos;

		explicit constexpr RuntimeFieldInfo(const Instruction _i) : fieldInfos(initFieldInfos(_i))
		{
		}

		template<uint32_t... F>
		constexpr static std::array<FieldInfo, Field_COUNT> initFieldInfos(const Instruction _instruction, std::integer_sequence<uint32_t, F...>)
		{
			return std::array<FieldInfo, Field_COUNT>{initField(g_opcodes[_instruction].m_opcode, g_fieldParseConfigs[F].ch, g_fieldParseConfigs[F].count)...};
		}
		constexpr static std::array<FieldInfo, Field_COUNT> initFieldInfos(const Instruction _instruction)
		{
			return initFieldInfos(_instruction, std::make_integer_sequence<uint32_t, Field_COUNT>());
		}
	};

	struct RuntimeFieldInfos
	{
		const std::array<RuntimeFieldInfo, g_opcodeCount> fieldInfos;

		constexpr RuntimeFieldInfos() : fieldInfos(initFieldInfos())
		{
		}

		template<uint32_t... I>
		constexpr static std::array<RuntimeFieldInfo, g_opcodeCount> initFieldInfos(std::integer_sequence<uint32_t, I...>)
		{
			return std::array<RuntimeFieldInfo, g_opcodeCount>{RuntimeFieldInfo(static_cast<Instruction>(I))...};
		}
		constexpr static std::array<RuntimeFieldInfo, g_opcodeCount> initFieldInfos()
		{
			return initFieldInfos(std::make_integer_sequence<uint32_t, g_opcodeCount>());
		}
	};

	constexpr RuntimeFieldInfos g_runtimeFieldInfos;

	static_assert(getFieldInfoCE<Bsset_S, Field_DDDDDD>().bit == 8, "invalid");

	const FieldInfo& getFieldInfo(const Instruction _i, const Field _f)
	{
		return g_runtimeFieldInfos.fieldInfos[_i].fieldInfos[_f];
	}

	Opcodes::Opcodes()
	{
		constexpr auto len = g_opcodeCount;

		m_opcodesAlu.reserve(len);
		m_opcodesMove.reserve(len);
		m_opcodesNonParallel.reserve(len);

		for(size_t i=0; i<len; ++i)
		{
			const auto& opcode = g_opcodes[i];

			if(opcode.getInstruction() == ResolveCache)
				continue;

			assert(opcode.getInstruction() == i && "programming error, list sorting is faulty");

			if(dsp56k::isNonParallelOpcode(opcode))
				m_opcodesNonParallel.push_back(&opcode);
			else if(hasField(opcode, Field_AluOperation))
				m_opcodesMove.push_back(&opcode);
			else if(hasField(opcode, Field_MoveOperation))
				m_opcodesAlu.push_back(&opcode);
		}
	}

	const OpcodeInfo* Opcodes::findNonParallelOpcodeInfo(TWord _opcode) const
	{
		assert(isNonParallelOpcode(_opcode));
		return findOpcodeInfo(_opcode, m_opcodesNonParallel);
	}

	const OpcodeInfo* Opcodes::findParallelMoveOpcodeInfo(TWord _opcode) const
	{
		assert(isParallelOpcode(_opcode));
		return findOpcodeInfo(_opcode, m_opcodesMove);
	}

	const OpcodeInfo* Opcodes::findParallelAluOpcodeInfo(TWord _opcode) const
	{
		assert(isParallelOpcode(_opcode));
		return findOpcodeInfo(_opcode, m_opcodesAlu);
	}

	const OpcodeInfo& Opcodes::getOpcodeInfoAt(size_t _index)
	{
		return g_opcodes[_index];
	}

	const OpcodeInfo* Opcodes::findOpcodeInfo(TWord _opcode, const std::vector<const OpcodeInfo*>& _opcodes)
	{
		const OpcodeInfo* res = nullptr;

		for (size_t i=0; i<_opcodes.size(); ++i)
		{
			const auto* oi = _opcodes[i];

			if(match(*oi, _opcode))
			{
				if(res != nullptr)
				{
					match(*oi, _opcode);
					match(*res, _opcode);
					assert(res == nullptr);
				}
				res = oi;
			}
		}
		return res;
	}
}
