#pragma once

#include "jitcacheentry.h"
#include "types.h"

#include <vector>
#include <set>

#include "jitruntimedata.h"

#include "logging.h"

namespace asmjit
{
	inline namespace _abi_1_8
	{
		class JitRuntime;
	}
}

namespace dsp56k
{
	class DSP;
	class JitBlock;

	class Jit final
	{
	public:
		explicit Jit(DSP& _dsp);
		~Jit();

		DSP& dsp() { return m_dsp; }

		void exec(const TWord _pc)
		{
//			LOG("Exec @ " << HEX(pc));

			// get JIT code
			exec(_pc, m_jitFuncs[_pc]);
		}

		void notifyProgramMemWrite(const TWord _offset)
		{
			destroy(_offset);
		}

		void run(TWord _pc);
		void runCheckPMemWrite(TWord _pc);
		void create(TWord _pc, bool _execute);
		void recreate(TWord _pc);

		JitBlock* getChildBlock(JitBlock* _parent, TWord _pc, bool _allowCreate = true);
		bool canBeDefaultExecuted(TWord _pc) const;

		void occupyArea(JitBlock* _block);

	private:
		void emit(TWord _pc);
		void destroyParents(JitBlock* _block);
		void destroy(JitBlock* _block);
		void destroy(TWord _pc)
		{
			const auto block = m_jitCache[_pc].block;
			if (block)
				destroy(block);
		}
		void release(const JitBlock* _block);
		bool isBeingGeneratedRecursive(const JitBlock* _block) const;
		bool isBeingGenerated(const JitBlock* _block) const;

		void exec(TWord _pc, const TJitFunc& _f)
		{
			_f(this, _pc);
		}

		static TJitFunc updateRunFunc(const JitCacheEntry& e);

		void checkPMemWrite();

		JitRuntimeData m_runtimeData;

		DSP& m_dsp;

		asmjit::_abi_1_8::JitRuntime* m_rt = nullptr;
		std::vector<JitCacheEntry> m_jitCache;
		std::vector<TJitFunc> m_jitFuncs;
		std::set<TWord> m_volatileP;
		std::map<TWord, JitBlock*> m_generatingBlocks;
		size_t m_codeSize = 0;
	};
}
