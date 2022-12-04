#pragma once

#include <vector>

#include "jitcacheentry.h"
#include "jitdspmode.h"

namespace dsp56k
{
	class DSP;

	class JitBlockChain final
	{
	public:
		JitBlockChain(Jit& _jit, const JitDspMode& _mode);
		~JitBlockChain();

		bool canBeDefaultExecuted(TWord _pc) const;

		void create(TWord _pc, bool _execute);
		void recreate(TWord _pc);
		void destroy(TWord _pc);

		JitBlock* getChildBlock(JitBlock* _parent, TWord _pc, bool _allowCreate = true);
		JitBlock* emit(TWord _pc);

		JitBlock* getBlock(const TWord _pc)
		{
			return m_jitCache[_pc].block;
		}

		const TJitFunc& getFunc(const TWord _pc) const
		{
			return m_jitFuncs[_pc];
		}

		void exec(const TWord _pc) const
		{
			exec(_pc, m_jitFuncs[_pc]);
		}

		void exec(const TWord _pc, const TJitFunc& _f) const
		{
			_f(&m_jit, _pc);
		}

		const JitDspMode& getMode() const
		{
			return m_mode;
		}

	private:

		void destroyParents(JitBlock* _block);
		void destroy(JitBlock* _block);

		void release(const JitBlock* _block);
		void occupyArea(JitBlock* _block);
		void unoccupyArea(const JitBlock* _block);

		bool isBeingGeneratedRecursive(const JitBlock* _block) const;
		bool isBeingGenerated(const JitBlock* _block) const;

		Jit& m_jit;
		const JitDspMode m_mode;

		std::vector<JitCacheEntry> m_jitCache;
		std::vector<TJitFunc> m_jitFuncs;

		std::map<TWord, JitBlock*> m_generatingBlocks;

		size_t m_codeSize = 0;
	};
}
