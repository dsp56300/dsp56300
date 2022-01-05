#include "opcodes.h"

namespace dsp56k
{
	struct RuntimeFieldInfo
	{
		FieldInfo fieldInfos[Field_COUNT];

		constexpr RuntimeFieldInfo() : fieldInfos() {}
		constexpr RuntimeFieldInfo(const Instruction _i) : fieldInfos()
		{
			for (auto f = 0; f < Field::Field_COUNT; ++f)
				fieldInfos[f] = initField(g_opcodes[_i].m_opcode, g_fieldParseConfigs[f].ch, g_fieldParseConfigs[f].count);
		}
	};

	struct RuntimeFieldInfos
	{
		RuntimeFieldInfo fieldInfos[g_opcodeCount];

		constexpr RuntimeFieldInfos() : fieldInfos()
		{
			for (size_t i = 0; i < g_opcodeCount; ++i)
				fieldInfos[i] = RuntimeFieldInfo(static_cast<Instruction>(i));
		}
	};

	const RuntimeFieldInfos g_runtimeFieldInfos;

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
