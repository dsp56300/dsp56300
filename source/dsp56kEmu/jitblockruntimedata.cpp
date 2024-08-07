#include "jitblockruntimedata.h"

#include "asmjit/core/codeholder.h"

namespace dsp56k
{
	JitBlockRuntimeData::~JitBlockRuntimeData()
	{
		assert(m_generating == false);
	}

	uint64_t JitBlockRuntimeData::getSingleOpCacheKey(const TWord _opA, const TWord _opB)
	{
		uint64_t res = _opA;
		res <<= 32;
		res |= _opB;
		return res;
	}

	void JitBlockRuntimeData::finalize(const TJitFunc& _func, const asmjit::CodeHolder& _codeHolder)
	{
		m_func = _func;
		m_codeSize = _codeHolder.codeSize();

		for (auto& pi : m_profilingInfo)
		{
			pi.codeOffset = _codeHolder.labelOffset(pi.labelBefore);
			pi.codeOffsetAfter = _codeHolder.labelOffset(pi.labelAfter);
		}
	}

	void JitBlockRuntimeData::reset()
	{
		m_func = nullptr;

		m_lastOpSize = 0;
		m_singleOpWordA = 0;
		m_singleOpWordB = 0;
		m_encodedInstructionCount = 0;
		m_encodedCycles = 0;

		m_dspAsm.clear();
		m_child = g_invalidAddress;			// JIT block that we call
		m_nonBranchChild = g_invalidAddress;
		m_codeSize = 0;

		m_info.reset();

		m_parents.clear();
		m_generating = false;
		m_profilingInfo.clear();
	}

	void JitBlockRuntimeData::addParent(const TWord _pc)
	{
		m_parents.insert(_pc);
	}

}
