#pragma once

#include <set>
#include <vector>

#include "asmjit/core/api-config.h"

namespace asmjit
{
	inline namespace ASMJIT_ABI_NAMESPACE
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
