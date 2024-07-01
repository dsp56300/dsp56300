#pragma once

#include <memory>
#include <vector>

#include "jitcacheentry.h"
#include "jitdspmode.h"
#include "jittypes.h"

namespace dsp56k
{
	class AsmJitLogger;
	class AsmJitErrorHandler;
	class DSP;
	class JitBlockRuntimeData;

	class JitBlockChain final
	{
	public:
		JitBlockChain(Jit& _jit, const JitDspMode& _mode, size_t _usedFuncSize);
		~JitBlockChain();

		bool canBeDefaultExecuted(TWord _pc) const;

		void create(TWord _pc, bool _execute);
		void recreate(TWord _pc);
		void destroy(TWord _pc);
		void destroyToRecreate(TWord _pc);

		JitBlockRuntimeData* getChildBlock(JitBlockRuntimeData* _parent, TWord _pc, bool _allowCreate = true);
		JitBlockRuntimeData* emit(TWord _pc);

		JitBlockRuntimeData* getBlock(const TWord _pc) const
		{
			if(_pc >= m_jitCache.size())
				return nullptr;

			return getBlockUnsafe(_pc);
		}

		JitBlockRuntimeData* getBlockUnsafe(const TWord _pc) const
		{
			return m_jitCache[_pc].block;
		}

		const auto& getFuncs() const
		{
			return m_jitFuncs;
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

		void notifyPMemWrite(TWord _addr, bool _isCurrentChain);

		size_t getFuncSize() const
		{
			return m_jitFuncs.size();
		}

		void setMaxUsedPAddress(const size_t _maxUsedPAddress)
		{
			ensureFuncSize(_maxUsedPAddress);
		}

		Jit& getJit() const
		{
			return m_jit;
		}

	private:

		void destroyParents(JitBlockRuntimeData* _block);
		void destroy(JitBlockRuntimeData* _block);

		void release(JitBlockRuntimeData* _block);
		void occupyArea(JitBlockRuntimeData* _block);
		void unoccupyArea(const JitBlockRuntimeData* _block);

		bool isBeingGeneratedRecursive(const JitBlockRuntimeData* _block) const;
		bool isBeingGenerated(const JitBlockRuntimeData* _block) const;

		bool ensureCacheSize(size_t _address);
		bool ensureFuncSize(size_t _address);
		bool ensureSize(size_t _address);

		void onFuncsResized() const;

		Jit& m_jit;
		const JitDspMode m_mode;

		std::vector<JitCacheEntry> m_jitCache;
		std::vector<TJitFunc> m_jitFuncs;

		std::map<TWord, JitBlockRuntimeData*> m_generatingBlocks;

		std::unique_ptr<AsmJitLogger> m_logger;
		std::unique_ptr<AsmJitErrorHandler> m_errorHandler;

		size_t m_codeSize = 0;
	};
}
