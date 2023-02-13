#include "jitoptimizer.h"

#include "logging.h"

#include <asmjit/asmjit.h>

#include "jitemitter.h"

namespace dsp56k
{
	JitOptimizer::JitOptimizer(JitEmitter& _emitter) : m_asm(_emitter)
	{
	}

	void JitOptimizer::optimize(const JitEmitter& _emitter)
	{
		optimize(_emitter.firstNode());
	}

	void JitOptimizer::optimize(asmjit::BaseNode* _node)
	{
		m_ops.clear();

		LOG("BEGIN");

		size_t idx=0;

		while(_node)
		{
			if(!_node->isInst())
			{
				_node = _node->next();
				++idx;
				continue;
			}

			const auto* inst = _node->as<asmjit::InstNode>();
			asmjit::InstRWInfo rwInfo{};
			asmjit::InstAPI::queryRWInfo(asmjit::Environment::host().arch(), inst->baseInst(), inst->_opArray, inst->opCount(), &rwInfo);

			assert(rwInfo.opCount() == inst->opCount());

			for(size_t i=0; i<rwInfo.opCount(); ++i)
			{
				const auto& op = inst->operands()[i];

				if(!op.isReg())
					continue;

				const auto& reg = op.as<asmjit::BaseReg>();

				if(!reg.isGp() && !reg.isVec())
					continue;

				const asmjit::OpRWInfo& opInfo = rwInfo.operand(i);

				const auto rw = opInfo.isReadWrite();
				const auto written = rw || opInfo.isWrite();
				const auto read = rw || opInfo.isRead();

				const auto prefix = reg.isVec() ? "vec" : "gp";

				if(rw)
					LOG(idx << " rw " << prefix << reg._baseId)
				else if(written)
					LOG(idx << " write " << prefix << reg._baseId)
				else if(read)
					LOG(idx << " read " << prefix << reg._baseId)
			}

			_node = _node->next();
			++idx;
		}

		LOG("END");
	}
}
