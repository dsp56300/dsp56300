#pragma once

#include <map>
#include <set>
#include <vector>

#include "jittypes.h"

namespace asmjit
{
	inline namespace ASMJIT_ABI_NAMESPACE
	{
		class BaseNode;
		class InstNode;
		struct InstRWInfo;
	}
}

namespace dsp56k
{
	class JitEmitter;

	class JitOptimizer
	{
	public:
		JitOptimizer(JitEmitter& _emitter);

		size_t optimize();

	private:
		struct RegKey
		{
			uint32_t group;
			uint32_t id;

			bool operator<(const RegKey& _other) const
			{
				if(group != _other.group) return group < _other.group;
				return id < _other.id;
			}

			bool operator==(const RegKey& _other) const
			{
				return group == _other.group && id == _other.id;
			}
		};

		static RegKey cpuFlagsKey() { return {0xFFFFFFFF, 0}; }

		static bool getRegKey(RegKey& _result, const asmjit::Operand& _op);
		static int64_t maskToRegSize(const asmjit::Operand& _reg, int64_t _value);

		size_t removeDeadMovs();
		size_t deadCodeElimination();
		size_t constantFolding();

		bool isSideEffectFree(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo) const;
		bool isControlFlow(const asmjit::BaseNode* _node) const;
		bool areFlagsLive(const asmjit::BaseNode* _node) const;

		JitEmitter& m_asm;
	};
}
