#pragma once

#include <set>

#include "interrupts.h"
#include "jitblock.h"
#include "types.h"

namespace dsp56k
{
	class JitBlock;

	class JitBlockRuntimeData final
	{
	public:
		friend class JitBlock;

		static constexpr TWord SingleOpCacheIgnoreWordB = 0xffffffff;

		struct InstructionProfilingInfo
		{
			TWord pc = 0;
			TWord opA = 0, opB = 0;	// used for delayed disasm
			TWord opLen = 0;
			TWord lineCount = 1;
			asmjit::Label labelBefore, labelAfter;
			uint64_t codeOffset = 0;
			uint64_t codeOffsetAfter = 0;
			std::string sourceText;
		};

		JitBlockRuntimeData() = default;
		~JitBlockRuntimeData();

		const std::string& getDisasm() const { return m_dspAsm; }
		TWord getLastOpSize() const { return m_lastOpSize; }
		TWord getSingleOpWordA() const { return m_singleOpWordA; }
		TWord getSingleOpWordB() const { return m_singleOpWordB; }

		uint64_t getSingleOpCacheKey() const
		{
			return getSingleOpCacheKey(m_singleOpWordA, getPMemSize() == 1 ? SingleOpCacheIgnoreWordB : m_singleOpWordB);
		}

		void setGenerating(bool _generating)
		{
			m_generating = _generating;
		}

		static uint64_t getSingleOpCacheKey(TWord _opA, TWord _opB);

		TWord getChild() const { return m_child; }
		TWord getNonBranchChild() const { return m_nonBranchChild; }
		size_t codeSize() const { return m_codeSize; }
		const std::set<TWord>& getParents() const { return m_parents; }
		void clearParents() { m_parents.clear(); }

		TWord getPCFirst() const { return m_info.pc; }
		TWord getPMemSize() const { return m_info.memSize; }
		TWord getPCNext() const { return getPCFirst() + getPMemSize(); }

		bool isFastInterrupt() const { return getPCFirst() < Vba_End; }

		void finalize(const TJitFunc& _func, const asmjit::CodeHolder& _codeHolder);

		const TJitFunc& getFunc() const { return m_func; }

		TWord& getEncodedInstructionCount() { return m_encodedInstructionCount; }
		TWord& getEncodedCycleCount() { return m_encodedCycles; }

		std::vector<InstructionProfilingInfo>& getProfilingInfo() { return m_profilingInfo; }
		size_t getCodeSize() const { return m_codeSize; }

		const JitBlockInfo& getInfo() const { return m_info; }

		void reset();

	private:
		void addParent(TWord _pc);

		TJitFunc m_func = nullptr;

		TWord m_lastOpSize = 0;
		TWord m_singleOpWordA = 0;
		TWord m_singleOpWordB = 0;
		TWord m_encodedInstructionCount = 0;
		TWord m_encodedCycles = 0;

		std::string m_dspAsm;
		bool m_possibleBranch = false;
		TWord m_child = g_invalidAddress;			// JIT block that we call
		TWord m_nonBranchChild = g_invalidAddress;
		size_t m_codeSize = 0;

		JitBlockInfo m_info;

		std::set<TWord> m_parents;
		bool m_generating = false;
		std::vector<InstructionProfilingInfo> m_profilingInfo;
	};
}
