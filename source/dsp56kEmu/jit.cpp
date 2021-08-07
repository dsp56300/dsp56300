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
		_jit->create(_pc, _block);
	}

	void funcRecreate(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->recreate(_pc, _block);
	}

	void funcRunCheckPMemWrite(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->runCheckPMemWrite(_pc, _block);
	}

	void funcRunCheckLoopEnd(Jit* _jit, TWord _pc, JitBlock* _block)
	{
		_jit->runCheckLoopEnd(_pc, _block);
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
				m_rt->release(it->second->getFunc());
				delete it->second;
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
		m_dsp.m_instructions += m_runtimeData.m_executedInstructionCount;
	}

	void Jit::notifyProgramMemWrite(TWord _offset)
	{
		destroy(_offset);
	}

	void Jit::emit(const TWord _pc)
	{
		AsmJitLogger logger;
		AsmJitErrorHandler errorHandler;
		CodeHolder code;

//		code.setLogger(&logger);
		code.setErrorHandler(&errorHandler);
		code.init(m_rt->environment());

		JitEmitter m_asm(&code);

		auto* b = new JitBlock(m_asm, m_dsp, m_runtimeData);

		if(!b->emit(_pc, m_jitCache, m_volatileP))
		{
			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			delete b;
			return;
		}

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

		b->setFunc(func);

		const auto first = b->getPCFirst();
		const auto last = first + b->getPMemSize();

		for(auto i=first; i<last; ++i)
		{			
			m_jitCache[i].block = b;
			if(i == first)
				updateRunFunc(m_jitCache[i]);
			else
				m_jitCache[i].func = &funcRecreate;
		}

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
		if(iJIT_IsProfilingActive() == iJIT_SAMPLING_ON)
		{
			iJIT_Method_Load jmethod = {0};
			jmethod.method_id = iJIT_GetNewMethodID();
			std::stringstream ss;
			char temp[64];
			sprintf(temp, "$%06x-$%06x", first, last-1);
			if(b->getFlags() & JitBlock::LoopEnd)
				strcat(temp, " L");
			if(b->getFlags() & JitBlock::WritePMem)
				strcat(temp, " P");
			jmethod.method_name = temp;
			jmethod.class_file_name = const_cast<char*>("dsp56k::Jit");
			jmethod.source_file_name = __FILE__;
			jmethod.method_load_address = static_cast<void*>(func);
			jmethod.method_size = static_cast<unsigned int>(code.codeSize());

			iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &jmethod);
		}
#endif

//		LOG("New block generated @ " << HEX(_pc) << " up to " << HEX(_pc + b->getPMemSize() - 1) << ", instruction count " << b->getEncodedInstructionCount() << ", disasm " << b->getDisasm());
	}

	void Jit::destroy(JitBlock* _block)
	{
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
		}

		m_rt->release(_block->getFunc());

		delete _block;
	}

	void Jit::destroy(TWord _pc)
	{
		const auto block = m_jitCache[_pc].block;
		if(!block)
			return;
		destroy(block);
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
			if(_block->getDisasm().find("rep ") == 0)
				m_dsp.traceOp(lastPC, op, opB, _block->getLastOpSize());
		}
	}

	void Jit::runCheckPMemWrite(TWord _pc, JitBlock* _block)
	{
		m_runtimeData.m_pMemWriteAddress = g_pcInvalid;
		run(_pc, _block);

		// if JIT code has written to P memory, destroy a JIT block if present at the write location
		const TWord pMemWriteAddr = _block->pMemWriteAddress();
		if(pMemWriteAddr != g_pcInvalid)
		{
			if (m_jitCache[pMemWriteAddr].block)
				m_volatileP.insert(pMemWriteAddr);

			notifyProgramMemWrite(_block->pMemWriteAddress());
			m_dsp.notifyProgramMemWrite(_block->pMemWriteAddress());
		}
	}

	void Jit::runCheckLoopEnd(TWord _pc, JitBlock* _block)
	{
		run(_pc, _block);

		// loop processing
		if(m_dsp.sr_test(SR_LF))
		{
			if(m_dsp.getPC().var == m_dsp.regs().la.var + 1)
			{
				assert((_block->getFlags() & JitBlock::LoopEnd) != 0);
				auto& lc = m_dsp.regs().lc.var;
				if(lc <= 1)
				{
					m_dsp.do_end();
				}
				else
				{
					--lc;
					m_dsp.setPC(hiword(m_dsp.regs().ss[m_dsp.ssIndex()]));
				}
			}
		}
	}

	void Jit::create(const TWord _pc, JitBlock* _block)
	{
		auto& cacheEntry = m_jitCache[_pc];

		if(m_jitCache[_pc+1].block != nullptr)
		{
			// we will generate a 1-word op, try to find in single op cache
			TWord opA;
			TWord opB;
			m_dsp.memory().getOpcode(_pc, opA, opB);

			auto it = cacheEntry.singleOpCache.find(opA);

			if(it != cacheEntry.singleOpCache.end())
			{
//				LOG("Returning 1-word-op " << HEX(opA) << " at PC " << HEX(_pc));
				assert(cacheEntry.block == nullptr);
				cacheEntry.block = it->second;
				cacheEntry.singleOpCache.erase(it);
				updateRunFunc(cacheEntry);
				exec(_pc, cacheEntry);
				return;
			}
		}
		emit(_pc);
		exec(_pc, cacheEntry);
	}

	void Jit::recreate(const TWord _pc, JitBlock* _block)
	{
		// there is code, but the JIT block does not start at the PC position that we want to run. We need to throw the block away and regenerate
//		LOG("Unable to jump into the middle of a block, destroying existing block & recreating from " << HEX(pc));
		destroy(_block);
		create(_pc, _block);
	}

	void Jit::updateRunFunc(JitCacheEntry& e)
	{
		const auto f = e.block->getFlags();

		if(f & JitBlock::WritePMem)
		{
			assert((f & JitBlock::LoopEnd) == 0);
			e.func = &funcRunCheckPMemWrite;
		}
		else if(f & JitBlock::LoopEnd)
		{
			assert((f & JitBlock::WritePMem) == 0);
			e.func = &funcRunCheckLoopEnd;
		}
		else
		{
			if (g_traceOps)
				e.func = &funcRun;
			else
				e.func = e.block->getFunc();
		}
	}
}
