#include "opcodes.h"
#include "opcodeanalysis.h"

namespace dsp56k
{
	struct RuntimeFieldInfo
	{
		FieldInfo fieldInfos[Field_COUNT];

		RuntimeFieldInfo() : fieldInfos() {}
		RuntimeFieldInfo(const Instruction _i) : fieldInfos()
		{
			for (auto f = 0; f < Field::Field_COUNT; ++f)
				fieldInfos[f] = initField(g_opcodes[_i].m_opcode, g_fieldParseConfigs[f].ch, g_fieldParseConfigs[f].count);
		}
	};

	struct RuntimeFieldInfos
	{
		RuntimeFieldInfo fieldInfos[g_opcodeCount];

		RuntimeFieldInfos() : fieldInfos()
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

	uint32_t Opcodes::getOpcodeLength(const TWord _op) const
	{
		if(!_op)
			return 1;

		Instruction instA, instB;
		getInstructionTypes(_op, instA, instB);

		return getOpcodeLength(_op, instA, instB);
	}

	uint32_t Opcodes::getOpcodeLength(const TWord _op, Instruction _instA, Instruction _instB)
	{
		const auto lenA = _instA != Invalid ? dsp56k::getOpcodeLength(_instA, _op) : 0;
		const auto lenB = _instB != Invalid ? dsp56k::getOpcodeLength(_instB, _op) : 0;
		return std::max(lenA, lenB);
	}

	bool Opcodes::writesToPMemory(const TWord _op) const
	{
		if(!_op)
			return false;

		if(isNonParallelOpcode(_op))
		{
			const auto oi = findNonParallelOpcodeInfo(_op);
			return oi ? dsp56k::writesToPMemory(oi->getInstruction(), _op) : false;
		}

		const auto* oiMove = findParallelMoveOpcodeInfo(_op);

		return oiMove && dsp56k::writesToPMemory(oiMove->getInstruction(), _op);
	}

	uint32_t Opcodes::getInstructionTypes(const TWord _op, Instruction& _a, Instruction& _b) const
	{
		if(!_op)
		{
			_a = Nop;
			_b = Invalid;
			return 1;
		}

		_a = _b = Invalid;

		if(isNonParallelOpcode(_op))
		{
			const auto oi = findNonParallelOpcodeInfo(_op);

			if(oi)
				_a = oi->getInstruction();
			return 1;
		}

		const auto* oiAlu = (_op & 0xff) ? findParallelAluOpcodeInfo(_op) : nullptr;
		const auto* oiMove = findParallelMoveOpcodeInfo(_op);

		uint32_t res = 0;

		if(oiAlu)
		{
			_a = oiAlu->getInstruction();
			++res;
		}

		if(oiMove)
		{
			++res;

			if(oiAlu)
				_b = oiMove->getInstruction();
			else
				_a = oiMove->getInstruction();
		}
		return res;
	}

	bool Opcodes::getRegisters(RegisterMask& _written, RegisterMask& _read, const TWord _opA) const
	{
		Instruction instA, instB;
		getInstructionTypes(_opA, instA, instB);
		return getRegisters(_written, _read, _opA, instA, instB);
	}

	bool Opcodes::getRegisters(RegisterMask& _written, RegisterMask& _read, const TWord _opA, const Instruction _instA, const Instruction _instB)
	{
		_written = _read = RegisterMask::None;

		if(_instA != Invalid && _instA != Nop)
		{
			dsp56k::getRegisters(_written, _read, _instA, _opA);
		}

		if(_instB != Invalid && _instB != Nop)
		{
			auto written = RegisterMask::None;
			auto read = RegisterMask::None;
			dsp56k::getRegisters(written, read, _instB, _opA);
			_written |= written;
			_read |= read;
		}

		return _written != RegisterMask::None || _read != RegisterMask::None;
	}

	uint32_t Opcodes::getFlags(Instruction _instA, Instruction _instB)
	{
		uint32_t flags = 0;
		if(_instA != Invalid)
			flags = g_opcodes[_instA].m_flags;
		if(_instB != Invalid)
			flags |= g_opcodes[_instB].m_flags;
		return flags;
	}

	bool Opcodes::getMemoryAddress(TWord& _addr, EMemArea& _area, const TWord opA, const TWord opB) const
	{
		Instruction instA, instB;
		getInstructionTypes(opA, instA, instB);
		if(dsp56k::getMemoryAddress(_addr, _area, instA, opA, opB))
			return true;
		if(instB != Invalid)
			return dsp56k::getMemoryAddress(_addr, _area, instB, opA, opB);
		return false;
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
