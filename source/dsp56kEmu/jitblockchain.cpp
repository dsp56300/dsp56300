#include "jitblockchain.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitemitter.h"
#include "jitprofilingsupport.h"
#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	void funcRun(Jit* _jit, TWord _pc);
	void funcCreate(Jit* _jit, TWord _pc);
	void funcRecreate(Jit* _jit, TWord _pc);

	JitBlockChain::JitBlockChain(Jit& _jit, const JitDspMode& _mode) : m_jit(_jit), m_mode(_mode)
	{
		const auto& mem = _jit.dsp().memory();
		const auto pSize = mem.sizeP();
		m_jitCache.resize(pSize);
		m_jitFuncs.resize(pSize, &funcCreate);
	}

	JitBlockChain::~JitBlockChain()
	{
		for (auto& e : m_jitCache)
		{
			if(e.block)
				destroy(e.block);

			if(e.singleOpCache)
			{
				for (const auto& it : *e.singleOpCache)
					release(it.second);

				e.singleOpCache->clear();
			}
		}

		m_jitCache.clear();
	}

	bool JitBlockChain::canBeDefaultExecuted(TWord _pc) const
	{
		const auto& e = m_jitCache[_pc];
		if (!e.block)
			return false;
		return m_jitFuncs[_pc] == e.block->getFunc() || m_jitFuncs[_pc] == &funcRun;
	}

	void JitBlockChain::occupyArea(JitBlock* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		for (auto i = first; i < last; ++i)
		{
			assert(m_jitCache[i].block == nullptr || m_jitCache[i].block == _block);
			m_jitCache[i].block = _block;
			if (i == first)
				m_jitFuncs[i] = Jit::updateRunFunc(m_jitCache[i]);
			else
				m_jitFuncs[i] = &funcRecreate;
		}

		m_jit.addLoop(_block->getInfo());
	}

	void JitBlockChain::unoccupyArea(const JitBlock* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		for(auto i=first; i<last; ++i)
		{
			m_jitCache[i].block = nullptr;
			m_jitFuncs[i] = &funcCreate;
		}

		const auto& info = _block->getInfo();
		m_jit.removeLoop(info);
	}

	void JitBlockChain::create(const TWord _pc, bool _execute)
	{
//		LOG("Create @ " << HEX(_pc));// << std::endl << cacheEntry.block->getDisasm());

		auto& cacheEntry = m_jitCache[_pc];

		if(cacheEntry.singleOpCache)
		{
			TWord opA;
			TWord opB;
			m_jit.dsp().memory().getOpcode(_pc, opA, opB);

			// try to find two-word op first
			auto key = JitBlock::getSingleOpCacheKey(opA, opB);

			auto it = cacheEntry.findSingleOp(key);

			uint32_t cacheEntryLen = 2;

			// if not found, try one-word op
			if(!cacheEntry.isValid(it))
			{
				key = JitBlock::getSingleOpCacheKey(opA, JitBlock::SingleOpCacheIgnoreWordB);
				it = cacheEntry.findSingleOp(key);
				cacheEntryLen = 1;
			}

			if(cacheEntry.isValid(it))
			{
				if(cacheEntryLen == 1 || m_jitCache[_pc+1].block == nullptr)
				{
//					LOG("Returning single-op " << HEX(opA) << " at PC " << HEX(_pc));
					assert(cacheEntry.block == nullptr);

					cacheEntry.block = it->second;
					cacheEntry.removeSingleOp(it);

					occupyArea(cacheEntry.block);

					if(_execute)
						exec(_pc);
					return;
				}
			}
		}

		emit(_pc);
		if(_execute)
			exec(_pc);
	}

	void JitBlockChain::destroyParents(JitBlock* _block)
	{
		for (const auto parent : _block->getParents())
		{
			auto& e = m_jitCache[parent];

			if (e.block)
				destroy(e.block);

			// single op cached entries that are calling the child block need to go, too. They have been created at a time where _block was not a volatile P block yet
			if(e.singleOpCache)
			{
				for(auto it = e.singleOpCache->begin(); it != e.singleOpCache->end();)
				{
					const auto* b = it->second;
					if (b->getChild() == _block->getPCFirst() || b->getNonBranchChild() == _block->getPCFirst())
					{
						release(b);
						e.singleOpCache->erase(it++);
					}
					else
					{
						++it;
					}
				}
			}
		}
		_block->clearParents();
	}

	void JitBlockChain::destroy(JitBlock* _block)
	{
		destroyParents(_block);

		unoccupyArea(_block);

		if(m_jit.getConfig().cacheSingleOpBlocks && _block->getPMemSize() <= 2 && _block->getEncodedInstructionCount() == 1)
		{
			// if a single-word-op, cache it
			const auto first = _block->getPCFirst();
			auto& cacheEntry = m_jitCache[first];
			const auto op = _block->getSingleOpCacheKey();

			if(!cacheEntry.containsSingleOp(op))
			{
//				LOG("Caching single-op block " << HEX(op) << " at PC " << HEX(first));

				cacheEntry.addSingleOp(op, _block);
				return;
			}
		}

//		LOG("Destroying JIT block at PC " << HEX(first) << ", length " << _block->getPMemSize() << ", asm = " << _block->getDisasm());

		release(_block);
	}

	void JitBlockChain::destroy(const TWord _pc)
	{
		const auto block = m_jitCache[_pc].block;
		if (block)
			destroy(block);
	}

	void JitBlockChain::release(const JitBlock* _block)
	{
#if DSP56300_DEBUGGER
		auto* d = m_jit.dsp().getDebugger();
		if(d)
			d->onJitBlockDestroyed(m_mode, _block->getInfo().pc);
#endif
		assert(m_codeSize >= _block->codeSize());
		m_codeSize -= _block->codeSize();
		m_jit.getRuntime()->release(_block->getFunc());
		delete _block;

//		LOG("Total code size now " << (m_codeSize >> 10) << "kb");
	}

	void JitBlockChain::recreate(const TWord _pc)
	{
		// there is code, but the JIT block does not start at the PC position that we want to run. We need to throw the block away and regenerate
//		LOG("Unable to jump into the middle of a block, destroying existing block & recreating from " << HEX(pc));
		m_jit.destroy(_pc);
		create(_pc, true);
	}

	JitBlock* JitBlockChain::getChildBlock(JitBlock* _parent, TWord _pc, bool _allowCreate/* = true*/)
	{
		if (!m_jit.getConfig().linkJitBlocks)
			return nullptr;

		if (_parent)
			occupyArea(_parent);

		if (m_jit.isVolatileP(_pc))
			return nullptr;

		const auto& e = m_jitCache[_pc];

		if (e.block && e.block->getPCFirst() == _pc)
		{
			// block is still being generated (circular reference)
			if (m_jitFuncs[_pc] == nullptr)
				return nullptr;

			if (!canBeDefaultExecuted(_pc))
				return nullptr;

			return e.block;
		}

		if (!_allowCreate)
			return nullptr;

		// If we jump into the middle of an existing block, this block needs to be regenerated.
		// However, we can only destroy blocks that are not part of the recursive generation that is running at the moment
		if (isBeingGeneratedRecursive(e.block))
			return nullptr;

		// regenerate otherwise
		if (e.block)
			destroy(e.block);

		create(_pc, false);

		if (!canBeDefaultExecuted(_pc))
			return nullptr;
		return e.block;
	}

	JitBlock* JitBlockChain::emit(TWord _pc)
	{
		AsmJitLogger logger;
		logger.addFlags(asmjit::FormatFlags::kHexImms | /*asmjit::FormatFlags::kHexOffsets |*/ asmjit::FormatFlags::kMachineCode);
		AsmJitErrorHandler errorHandler;
		asmjit::CodeHolder code;

		code.setErrorHandler(&errorHandler);
		code.init(m_jit.getRuntime()->environment());

		JitEmitter m_asm(&code);

//		code.setLogger(&logger);
//		m_asm.addDiagnosticOptions(DiagnosticOptions::kValidateIntermediate);
//		m_asm.addDiagnosticOptions(DiagnosticOptions::kValidateAssembler);
		
		auto* b = new JitBlock(m_asm, m_jit.dsp(), m_jit.getRuntimeData(), m_jit.getConfig());

		errorHandler.setBlock(b);

		m_generatingBlocks.insert(std::make_pair(_pc, b));

		if(!b->emit(this, _pc, m_jitCache, m_jit.getVolatileP(), m_jit.getLoops(), m_jit.getLoopEnds(), m_jit.getProfilingSupport()))
		{
			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			delete b;
			m_generatingBlocks.erase(_pc);
			return nullptr;
		}

		m_generatingBlocks.erase(_pc);

		m_asm.ret();

		m_asm.finalize();

		TJitFunc func;

		const auto err = m_jit.getRuntime()->add(&func, &code);

		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return nullptr;
		}

		b->finalize(func, code);
		m_codeSize += code.codeSize();

//		LOG("Total code size now " << (m_codeSize >> 10) << "kb");

		occupyArea(b);

		auto* profiling = m_jit.getProfilingSupport();
		if (profiling)
			profiling->addJitBlock(*b);

#if DSP56300_DEBUGGER
		auto* d = m_jit.dsp().getDebugger();
		if(d)
			d->onJitBlockCreated(m_mode, b);
#endif
		return b;
	}

	bool JitBlockChain::isBeingGeneratedRecursive(const JitBlock* _block) const
	{
		if (!_block)
			return false;

		if (isBeingGenerated(_block))
			return true;

		for (const auto parent : _block->getParents())
		{
			const auto& e = m_jitCache[parent];
			if (isBeingGeneratedRecursive(e.block))
				return true;
		}
		return false;
	}

	bool JitBlockChain::isBeingGenerated(const JitBlock* _block) const
	{
		if (_block == nullptr)
			return false;

		for (const auto it : m_generatingBlocks)
		{
			if (it.second == _block)
				return true;
		}
		return false;
	}
}
