#include "opcodes.h"

namespace dsp56k
{
	/*
		{
			initField(_opcode, 'a', 1),		// Field_a
			initField(_opcode, 'a', 3),		// Field_aaa
			initField(_opcode, 'a', 4),		// Field_aaaa
			initField(_opcode, 'a', 5),		// Field_aaaaa
			initField(_opcode, 'a', 6),		// Field_aaaaaa
			initField(_opcode, 'a', 12),	// Field_aaaaaaaaaaaa
			initField(_opcode, 'b', 5),		// Field_bbbbb
			initField(_opcode, 'C', 4),		// Field_CCCC
			initField(_opcode, 'd', 1),		// Field_d
			initField(_opcode, 'd', 2),		// Field_dd
			initField(_opcode, 'd', 3),		// Field_ddd,			
			initField(_opcode, 'd', 4),		// Field_dddd,			
			initField(_opcode, 'd', 5),		// Field_ddddd,		
			initField(_opcode, 'd', 6),		// Field_dddddd,		
			initField(_opcode, 'D', 1),		// Field_D,			
			initField(_opcode, 'D', 4),		// Field_DDDD,			
			initField(_opcode, 'D', 5),		// Field_DDDDD,		
			initField(_opcode, 'D', 6),		// Field_DDDDDD,		
			initField(_opcode, 'e', 1),		// Field_e,			
			initField(_opcode, 'e', 2),		// Field_ee,			
			initField(_opcode, 'E', 2),		// Field_EE,			
			initField(_opcode, 'f', 2),		// Field_eeeee,		
			initField(_opcode, 'F', 1),		// Field_eeeeee,		
			initField(_opcode, 'e', 5),		// Field_ff,			
			initField(_opcode, 'e', 6),		// Field_F,			
			initField(_opcode, 'g', 3),		// Field_ggg,			
			initField(_opcode, 'h', 4),		// Field_hhhh,			
			initField(_opcode, 'i', 1),		// Field_i,			
			initField(_opcode, 'i', 5),		// Field_iiiii,		
			initField(_opcode, 'i', 6),		// Field_iiiiii,		
			initField(_opcode, 'i', 8),		// Field_iiiiiiii,		
			initField(_opcode, 'J', 1),		// Field_J,			
			initField(_opcode, 'J', 2),		// Field_JJ,			
			initField(_opcode, 'J', 3),		// Field_JJJ,			
			initField(_opcode, 'k', 1),		// Field_k,			
			initField(_opcode, 'L', 1),		// Field_L,			
			initField(_opcode, 'L', 2),		// Field_LL,			
			initField(_opcode, 'm', 2),		// Field_mm,			
			initField(_opcode, 'M', 2),		// Field_MM,			
			initField(_opcode, 'M', 3),		// Field_MMM,			
			initField(_opcode, 'p', 6),		// Field_pppppp,		
			initField(_opcode, 'q', 2),		// Field_qq,			
			initField(_opcode, 'q', 3),		// Field_qqq,			
			initField(_opcode, 'q', 1),		// Field_q,			
			initField(_opcode, 'q', 5),		// Field_qqqqq,		
			initField(_opcode, 'q', 6),		// Field_qqqqqq,		
			initField(_opcode, 'Q', 2),		// Field_QQ,			
			initField(_opcode, 'Q', 3),		// Field_QQQ,			
			initField(_opcode, 'Q', 4),		// Field_QQQQ,			
			initField(_opcode, 'r', 2),		// Field_rr,			
			initField(_opcode, 'R', 3),		// Field_RRR,			
			initField(_opcode, 's', 1),		// Field_s,			
			initField(_opcode, 's', 3),		// Field_sss,			
			initField(_opcode, 's', 4),		// Field_ssss,			
			initField(_opcode, 's', 5),		// Field_sssss,		
			initField(_opcode, 'S', 1),		// Field_S,			
			initField(_opcode, 'S', 3),		// Field_SSS,			
			initField(_opcode, 't', 3),		// Field_ttt,			
			initField(_opcode, 'T', 3),		// Field_TTT,			
			initField(_opcode, 'w', 1),		// Field_w,			
			initField(_opcode, 'W', 1),		// Field_W,			
			initField(_opcode, 'l', 1),		// Field_l,		
			initField(_opcode, 'o', 1),		// Field_o,			
			initField(_opcode, 'o', 2),		// Field_oo,		
			initField(_opcode, 'o', 3),		// Field_ooo,			
			initField(_opcode, 'o', 4),		// Field_oooo,			
			initField(_opcode, 'o', 5),		// Field_ooooo,			
			initField(_opcode, 'o', 6),		// Field_oooooo,	
			initField(_opcode, '?', 8),		// Field_AluOperation		
			initField(_opcode, '?', 16)		// Field_MoveOperation
		}
	*/

	struct RuntimeFieldInfo
	{
		FieldInfo m_fieldInfos[Field_COUNT];

		constexpr RuntimeFieldInfo() : m_fieldInfos() {}
		constexpr RuntimeFieldInfo(const Instruction _i) : m_fieldInfos()
		{
			for(auto f=0; f<Field::Field_COUNT; ++f)
				m_fieldInfos[f] = initField(g_opcodes[_i].m_opcode, g_fieldParseConfigs[f].ch, g_fieldParseConfigs[f].count);
		}
	};

	struct RuntimeFieldInfos
	{
		RuntimeFieldInfo m_fieldInfos[g_opcodeCount];

		constexpr RuntimeFieldInfos() : m_fieldInfos()
		{
			for(size_t i=0; i<g_opcodeCount; ++i)
				m_fieldInfos[i] = RuntimeFieldInfo(static_cast<Instruction>(i));
		}
	};

	RuntimeFieldInfos g_runtimeFieldInfos;

	static_assert(getFieldInfoCE<Bsset_S, Field_DDDDDD>().bit == 8, "invalid");

	const FieldInfo& getFieldInfo(Instruction _i, Field _f)
	{
		return g_runtimeFieldInfos.m_fieldInfos[_i].m_fieldInfos[_f];
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
		size_t resIndex = 0;

		for (size_t i=0; i<_opcodes.size(); ++i)
		{
			const auto* oi = _opcodes[i];

			if(match(*oi, _opcode))
			{
				if(res != nullptr)
					match(*oi, _opcode);
				assert(res == nullptr)
				res = oi;
				resIndex = i;
			}
		}
		return res;
	}
}
