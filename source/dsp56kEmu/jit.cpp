#include "jit.h"

#include "dsp.h"
#include "jitblock.h"
#include "jithelper.h"
#include "jitops.h"

#include "asmjit/core/jitruntime.h"

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
#include "../vtuneSdk/include/jitprofiling.h"
#endif

using namespace asmjit;

namespace dsp56k
{
	constexpr bool g_traceOps = false;

	void funcCreate(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->create(_pc, _block, true);
	}

	void funcRecreate(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->recreate(_pc, _block);
	}

	void funcRunCheckPMemWrite(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->runCheckPMemWrite(_pc, _block);
	}

	void funcRun(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->run(_pc, _block);
	}

	Jit::Jit(DSP& _dsp) : m_dsp(_dsp)
	{
		m_jitCache.resize(_dsp.memory().size(), JitCacheEntry{&funcCreate, nullptr});

		m_rt = new JitRuntime();
	}

	Jit::~Jit()
	{
		for(size_t i=0; i<m_jitCache.size(); ++i)
		{
			auto& e = m_jitCache[i];

			if(e.block)
				destroy(e.block);

			for(auto it = e.singleOpCache.begin(); it != e.singleOpCache.end(); ++it)
			{
				release(it->second);
			}
			e.singleOpCache.clear();
		}

		m_jitCache.clear();

		delete m_rt;
	}

	void Jit::exec(const TWord pc)
	{
//		LOG("Exec @ " << HEX(pc));

		// get JIT code
		auto& cacheEntry = m_jitCache[pc];
		exec(pc, cacheEntry);
	}

	void Jit::exec(const TWord pc, JitCacheEntry& e)
	{
		e.func(this, pc, e.block);
	}

	void Jit::notifyProgramMemWrite(TWord _offset)
	{
		destroy(_offset);
	}

	void Jit::emit(const TWord _pc)
	{
		AsmJitLogger logger;
		logger.addFlags(asmjit::FormatFlags::kHexImms | /*asmjit::FormatFlags::kHexOffsets |*/ asmjit::FormatFlags::kMachineCode);
		AsmJitErrorHandler errorHandler;
		CodeHolder code;

		code.setErrorHandler(&errorHandler);
		code.init(m_rt->environment());

		JitEmitter m_asm(&code);

//		code.setLogger(&logger);
//		m_asm.addDiagnosticOptions(DiagnosticOptions::kValidateIntermediate);
//		m_asm.addDiagnosticOptions(DiagnosticOptions::kValidateAssembler);
		
		auto* b = new JitBlock(m_asm, m_dsp, m_runtimeData);

		m_generatingBlocks.insert(std::make_pair(_pc, b));

		if(!b->emit(this, _pc, m_jitCache, m_volatileP))
		{
			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			delete b;
			m_generatingBlocks.erase(_pc);
			return;
		}

		m_generatingBlocks.erase(_pc);

		m_asm.ret();

		m_asm.finalize();

		JitBlock::JitEntry func;

		const auto err = m_rt->add(&func, &code);

		if(err)
		{
			const auto* const errString = DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		b->setFunc(func, code.codeSize());
		m_codeSize += code.codeSize();

//		LOG("Total code size now " << (m_codeSize >> 10) << "kb");

		occupyArea(b);

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
		if(iJIT_IsProfilingActive() == iJIT_SAMPLING_ON)
		{
			iJIT_Method_Load jmethod = {0};
			jmethod.method_id = iJIT_GetNewMethodID();
			char temp[64];
			sprintf(temp, "$%06x-$%06x", b->getPCFirst(), b->getPCFirst() + b->getPMemSize() - 1);
			if(b->getFlags() & JitBlock::LoopEnd)
				strcat(temp, " L");
			if(b->getFlags() & JitBlock::WritePMem)
				strcat(temp, " P");
			jmethod.method_name = temp;
			jmethod.class_file_name = const_cast<char*>("dsp56k::Jit");
			jmethod.source_file_name = temp;
			jmethod.method_load_address = static_cast<void*>(func);
			jmethod.method_size = static_cast<unsigned int>(code.codeSize());

			iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &jmethod);
		}
#endif

//		LOG("New block generated @ " << HEX(_pc) << " up to " << HEX(_pc + b->getPMemSize() - 1) << ", instruction count " << b->getEncodedInstructionCount() << ", disasm " << b->getDisasm());
	}

	void Jit::destroyParents(const JitBlock* _block)
	{
		for (const auto parent : _block->getParents())
		{
			auto& e = m_jitCache[parent];

			if (e.block)
				destroy(e.block);

			// single op cached entries that are calling the parent block need to go, too. They have been created at a time where _block was not a volatile P block yet
			for(auto it = e.singleOpCache.begin(); it != e.singleOpCache.end();)
			{
				const auto* b = it->second;
				if (b->getChild() == _block->getPCFirst())
				{
					release(b);
					e.singleOpCache.erase(it++);
				}
				else
				{
					++it;
				}
			}
		}
	}

	void Jit::destroy(JitBlock* _block)
	{
		destroyParents(_block);

		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

//		LOG("Destroying JIT block at PC " << HEX(first) << ", length " << _block->getPMemSize());

		for(auto i=first; i<last; ++i)
		{
			m_jitCache[i].block = nullptr;
			m_jitCache[i].func = &funcCreate;
		}

		if(_block->getPMemSize() == 1)
		{
			// if a 1-word-op, cache it
			auto& cacheEntry = m_jitCache[first];
			const auto op = _block->getSingleOpWord();

			if(cacheEntry.singleOpCache.find(op) == cacheEntry.singleOpCache.end())
			{
//				LOG("Caching 1-word-op " << HEX(opA) << " at PC " << HEX(first));

				cacheEntry.singleOpCache.insert(std::make_pair(op, _block));
				return;
			}
			destroyParents(_block);
		}

		release(_block);
	}

	void Jit::destroy(TWord _pc)
	{
		const auto block = m_jitCache[_pc].block;
		if(!block)
			return;
		destroy(block);
	}

	void Jit::release(const JitBlock* _block)
	{
		assert(m_codeSize >= _block->codeSize());
		m_codeSize -= _block->codeSize();
		m_rt->release(_block->getFunc());
		delete _block;

//		LOG("Total code size now " << (m_codeSize >> 10) << "kb");
	}

	bool Jit::isBeingGeneratedRecursive(const JitBlock* _block) const
	{
		if (!_block)
			return false;

		if (isBeingGenerated(_block))
			return true;

		for (const auto parent : _block->getParents())
		{
			const auto& e = m_jitCache[parent];
			if (isBeingGenerated(e.block))
				return true;
		}
		return false;
	}

	bool Jit::isBeingGenerated(const JitBlock* _block) const
	{
		if (_block == nullptr)
			return false;

		for (auto it : m_generatingBlocks)
		{
			if (it.second == _block)
				return true;
		}
		return false;
	}

	void Jit::run(TWord _pc, JitBlock* _block)
	{
		_block->getFunc()(this, _pc, _block);

		if(g_traceOps)
		{
			const TWord lastPC = _pc + _block->getPMemSize() - _block->getLastOpSize();
			TWord op, opB;
			m_dsp.mem.getOpcode(lastPC, op, opB);
			m_dsp.traceOp(lastPC, op, opB, _block->getLastOpSize());

			// make the diff tool happy, interpreter traces two ops. For the sake of simplicity, just trace it once more
//			if (_block->getDisasm().find("rep ") == 0)
//				m_dsp.traceOp(lastPC, op, opB, _block->getLastOpSize());
		}
	}

	void Jit::runCheckPMemWrite(TWord _pc, JitBlock* _block)
	{
		m_runtimeData.m_pMemWriteAddress = g_pcInvalid;
		run(_pc, _block);
		checkPMemWrite(_pc, _block);
	}

	void Jit::create(const TWord _pc, JitBlock*, bool _execute)
	{
//		LOG("Create @ " << HEX(_pc));// << std::endl << cacheEntry.block->getDisasm());

		auto& cacheEntry = m_jitCache[_pc];

		if(m_jitCache[_pc+1].block != nullptr)
		{
			// we will generate a 1-word op, try to find in single op cache
			TWord opA;
			TWord opB;
			m_dsp.memory().getOpcode(_pc, opA, opB);

			const auto it = cacheEntry.singleOpCache.find(opA);

			if(it != cacheEntry.singleOpCache.end())
			{
//				LOG("Returning 1-word-op " << HEX(opA) << " at PC " << HEX(_pc));
				assert(cacheEntry.block == nullptr);
				cacheEntry.block = it->second;
				cacheEntry.singleOpCache.erase(it);
				updateRunFunc(cacheEntry);
				if(_execute)
					exec(_pc, cacheEntry);
				return;
			}
		}
		emit(_pc);
		if(_execute)
			exec(_pc, cacheEntry);
	}

	void Jit::recreate(const TWord _pc, JitBlock* _block)
	{
		// there is code, but the JIT block does not start at the PC position that we want to run. We need to throw the block away and regenerate
//		LOG("Unable to jump into the middle of a block, destroying existing block & recreating from " << HEX(pc));
		destroy(_block);
		create(_pc, _block, true);
	}

	JitBlock* Jit::getChildBlock(JitBlock* _parent, TWord _pc, bool _allowCreate/* = true*/)
	{
		if (_parent)
			occupyArea(_parent);

		if (m_volatileP.find(_pc) != m_volatileP.end())
			return nullptr;

		const auto& e = m_jitCache[_pc];

		if (e.block && e.block->getPCFirst() == _pc)
		{
			// block is still being generated (circular reference)
			if (e.func == nullptr)
				return nullptr;

			if (!canBeDefaultExecuted(_pc))
				return nullptr;

			return e.block;
		}

		if (!_allowCreate)
			return nullptr;

		// If we jump in the middle of a block, this block needs to be regenerated.
		// We can only destroy blocks that are not part of the recursive generation at the moment
		if (isBeingGeneratedRecursive(e.block))
			return nullptr;

		// regenerate otherwise
		if (e.block)
			destroy(e.block);

		create(_pc, nullptr, false);

		if (!canBeDefaultExecuted(_pc))
			return nullptr;
		return e.block;
	}

	bool Jit::canBeDefaultExecuted(TWord _pc) const
	{
		const auto& e = m_jitCache[_pc];
		if (!e.block)
			return false;
		return e.func == e.block->getFunc();
	}

	void Jit::occupyArea(JitBlock* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		for (auto i = first; i < last; ++i)
		{
			assert(m_jitCache[i].block == nullptr || m_jitCache[i].block == _block);
			m_jitCache[i].block = _block;
			if (i == first)
				updateRunFunc(m_jitCache[i]);
			else
				m_jitCache[i].func = &funcRecreate;
		}
	}

	void Jit::updateRunFunc(JitCacheEntry& e)
	{
		const auto f = e.block->getFlags();

		if(f & JitBlock::WritePMem)
		{
			e.func = &funcRunCheckPMemWrite;
		}
		else
		{
			if (g_traceOps)
				e.func = &funcRun;
			else
				e.func = e.block->getFunc();
		}
	}

	void Jit::checkPMemWrite(TWord _pc, JitBlock* _block)
	{
		// if JIT code has written to P memory, destroy a JIT block if present at the write location
		const TWord pMemWriteAddr = _block->pMemWriteAddress();

		if (pMemWriteAddr == g_pcInvalid)
			return;

		if (m_jitCache[pMemWriteAddr].block)
			m_volatileP.insert(pMemWriteAddr);

		notifyProgramMemWrite(_block->pMemWriteAddress());
		m_dsp.notifyProgramMemWrite(_block->pMemWriteAddress());
	}
}
