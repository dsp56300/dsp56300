#pragma once

#include <set>
#include <vector>

namespace asmjit
{
	inline namespace _abi_1_9
	{
		class BaseNode;
	}
}

namespace dsp56k
{
	class JitEmitter;

	class JitOptimizer
	{
		struct Op
		{
			asmjit::BaseNode* node = nullptr;
		};

	public:
		JitOptimizer(JitEmitter& _emitter);

		void optimize(const JitEmitter& _emitter);
		void optimize(asmjit::BaseNode* _node);

	private:
		JitEmitter& m_asm;
		std::vector<Op> m_ops;
	};
}
