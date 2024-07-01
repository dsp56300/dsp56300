#include "jit.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitdspmode.h"
#include "jitprofilingsupport.h"
#include "jitblockemitter.h"

#include "asmjit/core/jitruntime.h"

#define WAIT_FOR_PROFILER 0

using namespace asmjit;

namespace dsp56k
{
#ifndef __ANDROID__
	// TODO: a.equals(b) is not a constant expression, android toolchain says. Maybe it needs an update?
	namespace
	{
		template<typename A, typename B> constexpr bool checkOverlap(const A& _a, const B& _b)
		{
			for (const auto& a : _a)
			{
				for (const auto& b : _b)
				{
					if (a.equals(b))
						return true;
				}
			}
			return false;
		}

		template<typename A, typename B> constexpr bool contains(const A& _a, const B& _b)
		{
			for (const auto& a : _a)
			{
				if (a.equals(_b))
					return true;
			}
			return false;
		}

		static_assert(!checkOverlap(g_dspPoolGps, g_regGPTemps), "GP temp registers must not overlap with GP pool registers");
		static_assert(!contains(g_dspPoolXmms, regXMMTempA), "XMM temp registers must not overlap with XMM pool registers");
		static_assert(!contains(g_dspPoolXmms, regLastModAlu), "XMM temp registers must not contain XMM that holds the last modified ALU reg");
		static_assert(!contains(g_dspPoolGps, regDspPtr), "GP pool registers must not contain GP that holds the dsp register pointer");
		static_assert(!contains(g_dspPoolGps, regReturnVal), "GP pool registers must not contain GP that is used as scratch register");
		static_assert(!checkOverlap(g_funcArgGPs, g_regGPTemps), "GP temp registers must not overlap with function argument GPs");
		static_assert(!checkOverlap(g_funcArgGPs, g_nonVolatileGPs), "function argument registers cannot be non-volatile");

		// these are important as we do not have to push anything on the stack for simple functions if we can use volatiles only
		static_assert(!contains(g_nonVolatileGPs, *g_regGPTemps.begin()), "first temp must be volatile");
		static_assert(!contains(g_nonVolatileGPs, regDspPtr), "register for DSP pointer must be volatile");
		static_assert(!contains(g_nonVolatileGPs, g_dspPoolGps[0]), "first pool reg must be volatile");
	}
#endif
	constexpr bool g_traceOps = false;

	void funcCreate(Jit* _jit, const TWord _pc)
	{
		_jit->create(_pc, true);
	}

	void funcRecreate(Jit* _jit, const TWord _pc)
	{
		_jit->recreate(_pc);
	}

	void funcRunCheckPMemWrite(Jit* _jit, const TWord _pc)
	{
		_jit->runCheckPMemWrite(_pc);
	}

	void funcRunCheckModeChange(Jit* _jit, const TWord _pc)
	{
		_jit->runCheckModeChange(_pc);
	}

	void funcRunCheckPMemWriteAndModeChange(Jit* _jit, const TWord _pc)
	{
		_jit->runCheckPMemWriteAndModeChange(_pc);
	}

	void funcRun(Jit* _jit, TWord _pc)
	{
		_jit->run(_pc);
	}

	Jit::Jit(DSP& _dsp) : m_dsp(_dsp), m_rt(new JitRuntime())
	{
		m_emitters.reserve(16);
		m_blockRuntimeDatas.reserve(0x10000);

#if WAIT_FOR_PROFILER
		// Wait for VTune profiler, somehow it does not immediately say that it is present
		const auto now = std::chrono::system_clock::now();
		while( std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - now) < std::chrono::milliseconds(10) )
		{
			if(JitProfilingSupport::isBeingProfiled())
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
#endif

		if (JitProfilingSupport::isBeingProfiled())
		{
			LOG("Detected profiler, generating additional data for it");
			try
			{
				m_profiling.reset(new JitProfilingSupport(m_dsp));
			}
			catch (const std::exception& e)
			{
				LOG("Failed to create profiler: " << e.what());
				m_profiling.reset();
			}
		}
		else
		{
			LOG("No profiler detected");
		}
	}

	Jit::~Jit()
	{
		m_chains.clear();

		for (const auto& emitter : m_emitters)
			delete emitter;
		for(const auto& rt : m_blockRuntimeDatas)
			delete rt;

		m_emitters.clear();
		m_blockRuntimeDatas.clear();

		delete m_rt;
	}

	void Jit::create(TWord _pc, bool _execute)
	{
		m_currentChain->create(_pc, _execute);
	}

	void Jit::recreate(TWord _pc)
	{
		m_currentChain->recreate(_pc);
	}

	void Jit::addLoop(const JitBlockInfo& _info)
	{
		if(_info.loopBegin != g_invalidAddress && _info.loopEnd != g_invalidAddress)
			addLoop(_info.loopBegin, _info.loopEnd);
	}

	void Jit::addLoop(TWord _begin, TWord _end)
	{
		// duplicated entries are allowed as the same code might be generated in multiple chains because it is run in different DSP modes. But in this case, the loop end must be identical
		const auto itBegin = m_loops.find(_begin);

		if(itBegin != m_loops.end())
		{
			assert(itBegin->second == _end);
			assert(m_loopEnds.find(itBegin->second) != m_loopEnds.end());
			return;
		}

		assert(m_loopEnds.find(_end) == m_loopEnds.end());

		m_loops.insert(std::make_pair(_begin, _end));
		m_loopEnds.insert(_end);
	}

	void Jit::removeLoop(const JitBlockInfo& _info)
	{
		if(_info.loopBegin != g_invalidAddress)
			removeLoop(_info.loopBegin);
	}

	void Jit::removeLoop(const TWord _begin)
	{
		const auto it = m_loops.find(_begin);

		// multiple chains might have contained the loop, if the first chain removed it already, it is already gone
		if(it == m_loops.end())
			return;

		assert(m_loopEnds.find(it->second) != m_loopEnds.end());

		m_loopEnds.erase(it->second);
		m_loops.erase(it);
	}

	void Jit::destroy(TWord _pc)
	{
		for (auto& it : m_chains)
			it.second->destroy(_pc);
	}

	void Jit::destroyToRecreate(TWord _pc)
	{
		for (auto& it : m_chains)
			it.second->destroyToRecreate(_pc);
	}

	void Jit::notifyProgramMemWrite(const TWord _offset)
	{
		for (auto& it : m_chains)
			it.second->notifyPMemWrite(_offset, it.second.get() == m_currentChain);

		m_maxUsedPAddress = std::max(m_maxUsedPAddress, static_cast<size_t>(_offset));
	}

	void Jit::run(const TWord _pc)
	{
		const auto* block = m_currentChain->getBlockUnsafe(_pc);
		block->getFunc()(this, _pc);

		if(g_traceOps && m_dsp.m_trace)
		{
			const TWord lastPC = _pc + block->getPMemSize() - block->getLastOpSize();
			TWord op, opB;
			m_dsp.mem.getOpcode(lastPC, op, opB);
			m_dsp.traceOp(lastPC, op, opB, block->getLastOpSize());

			// make the diff tool happy, interpreter traces two ops. For the sake of simplicity, just trace it once more
			if (block->getDisasm().find("rep ") == 0)
				m_dsp.traceOp(lastPC, op, opB, block->getLastOpSize());
		}
	}

	void Jit::runCheckPMemWrite(const TWord _pc)
	{
		m_runtimeData.m_pMemWriteAddress = g_pcInvalid;
		run(_pc);
		checkPMemWrite();
	}

	void Jit::runCheckPMemWriteAndModeChange(const TWord _pc)
	{
		runCheckPMemWrite(_pc);
		checkModeChange();
	}

	void Jit::runCheckModeChange(const TWord _pc)
	{
		run(_pc);
		checkModeChange();
	}

	void Jit::resetHW()
	{
		checkModeChange();
	}

	TJitFunc Jit::updateRunFunc(const JitCacheEntry& e)
	{
		const auto& i = e.block->getInfo();

		if(i.terminationReason == JitBlockInfo::TerminationReason::WritePMem)
		{
			if(i.hasFlag(JitBlockInfo::Flags::ModeChange))
				return &funcRunCheckPMemWriteAndModeChange;
			return &funcRunCheckPMemWrite;
		}

		if(i.hasFlag(JitBlockInfo::Flags::ModeChange))
			return &funcRunCheckModeChange;

		if (g_traceOps)
			return &funcRun;

		return e.block->getFunc();
	}

	void Jit::checkPMemWrite()
	{
		// if JIT code has written to P memory, destroy a JIT block if present at the write location
		const TWord pMemWriteAddr = m_runtimeData.m_pMemWriteAddress;

		if (pMemWriteAddr == g_pcInvalid)
			return;

		for (const auto& it : m_chains)
		{
			if (it.second->getBlock(pMemWriteAddr))
			{
				m_volatileP.insert(pMemWriteAddr);
				break;
			}
		}

		notifyProgramMemWrite(pMemWriteAddr);
		m_dsp.notifyProgramMemWrite(pMemWriteAddr);
	}

	void Jit::checkModeChange()
	{
		JitDspMode mode;

		mode.initialize(dsp());

		if(m_currentChain && m_currentChain->getMode() == mode)
			return;

//		LOG("DSP mode change to " << HEX(mode.get()));

		const auto itExisting = m_chains.find(mode);

		if(itExisting == m_chains.end())
		{
			m_currentChain = new JitBlockChain(*this, mode, m_maxUsedPAddress);
			m_chains.insert(std::make_pair(mode, m_currentChain));
		}
		else
		{
			m_currentChain = itExisting->second.get();
			m_currentChain->setMaxUsedPAddress(m_maxUsedPAddress);
		}

		m_dsp.setJitEntries(m_currentChain->getFuncs().data());
	}

	void Jit::onDebuggerAttached(DebuggerInterface& _debugger) const
	{
		for (auto& it : m_chains)
		{
			const auto mode = it.first;
			auto* chain = (it.second).get();

			const auto pSize = m_dsp.memory().sizeP();

			const JitBlockRuntimeData* last = nullptr;

			for(TWord pc=0; pc<pSize; ++pc)
			{
				const auto* block = chain->getBlock(pc);
				if(block != last && block)
					_debugger.onJitBlockCreated(mode, block);
				last = block;
			}
		}
	}

	void Jit::destroyAllBlocks()
	{
		m_chains.clear();
		m_currentChain = nullptr;
		checkModeChange();
	}

	JitBlockEmitter* Jit::acquireEmitter()
	{
		if(m_emitters.empty())
			return new JitBlockEmitter(dsp(), getRuntimeData(), getConfig());

		auto* emitter = m_emitters.back();
		m_emitters.pop_back();

		emitter->reset();

		return emitter;
	}

	void Jit::releaseEmitter(JitBlockEmitter* _emitter)
	{
		m_emitters.emplace_back(_emitter);
	}

	JitBlockRuntimeData* Jit::acquireBlockRuntimeData()
	{
		if(m_blockRuntimeDatas.empty())
			return new JitBlockRuntimeData();

		auto* r = m_blockRuntimeDatas.back();
		m_blockRuntimeDatas.pop_back();
		r->reset();
		return r;
	}

	void Jit::releaseBlockRuntimeData(JitBlockRuntimeData* _b)
	{
		m_blockRuntimeDatas.push_back(_b);
	}

	void Jit::onFuncsResized(const JitBlockChain& _chain) const
	{
		if(&_chain == m_currentChain)
		{
			m_dsp.setJitEntries(_chain.getFuncs().data());
		}
	}
}
