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

		// pool registers are handed out front to back, so every volatile must be listed before the first non-volatile
		template<typename A, typename B> constexpr bool volatilesFirst(const A& _pool, const B& _nonVolatiles)
		{
			bool seenNonVolatile = false;

			for (const auto& p : _pool)
			{
				bool isNonVolatile = false;

				for (const auto& nv : _nonVolatiles)
				{
					if (p.equals(nv))
					{
						isNonVolatile = true;
						break;
					}
				}

				if (isNonVolatile)
					seenNonVolatile = true;
				else if (seenNonVolatile)
					return false;
			}
			return true;
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
		static_assert(contains(g_nonVolatileGPs, regDspPtr), "register for DSP pointer must be non-volatile");
		static_assert(!contains(g_nonVolatileGPs, g_dspPoolGps[0]), "first pool reg must be volatile");
		static_assert(volatilesFirst(g_dspPoolGps, g_nonVolatileGPs), "GP pool registers must list all volatiles before the first non-volatile");
		static_assert(volatilesFirst(g_dspPoolXmms, g_nonVolatileXMMs), "XMM pool registers must list all volatiles before the first non-volatile");
	}
#endif
	constexpr bool g_traceOps = false;

	void funcCreate(JitDspPtr* _jit, const TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->create(_pc, true);
	}

	void funcRecreate(JitDspPtr* _jit, const TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->recreate(_pc);
	}

	void funcRunCheckPMemWrite(JitDspPtr* _jit, const TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->runCheckPMemWrite(_pc);
	}

	void funcRunCheckModeChange(JitDspPtr* _jit, const TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->runCheckModeChange(_pc);
	}

	void funcRunCheckPMemWriteAndModeChange(JitDspPtr* _jit, const TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->runCheckPMemWriteAndModeChange(_pc);
	}

	void funcRunCheckLoopEnd(JitDspPtr* _jit, const TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->runCheckLoopEnd(_pc);
	}

	void funcRun(JitDspPtr* _jit, TWord _pc) noexcept
	{
		Jit::toJitPtr(_jit)->run(_pc);
	}

	Jit::Jit(DSP& _dsp) : m_dsp(_dsp), m_trampoline(_dsp), m_rt(new JitRuntime())
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

		m_trampoline.generateCode();
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

		// The end can also differ when an LA write moved it while the loop ran, see checkLoopEnd. The registry keeps the
		// moved end then, and the DO block is compiled to check it when it runs.
		if(itBegin != m_loops.end())
		{
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

	Jit* Jit::toJitPtr(DspRegs* _regs)
	{
		const auto offsetRegs = offsetof(DSP, reg);
		const auto offsetJit = offsetof(DSP, m_jit);

		return reinterpret_cast<Jit*>(reinterpret_cast<uint8_t*>(_regs) + offsetJit - offsetRegs);
	}

	void Jit::notifyProgramMemWrite(const TWord _offset)
	{
		for (auto& it : m_chains)
			it.second->notifyPMemWrite(_offset, it.second.get() == m_currentChain);

		m_maxUsedPAddress = std::max(m_maxUsedPAddress, static_cast<size_t>(_offset));
	}

	void Jit::run(const TWord _pc) noexcept
	{
		const auto* block = m_currentChain->getBlockUnsafe(_pc);
		m_trampoline.execOne(&m_dsp.regs(), _pc, block->getFunc());

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

	void Jit::runCheckPMemWrite(const TWord _pc) noexcept
	{
		m_runtimeData.m_pMemWriteAddress = g_pcInvalid;
		run(_pc);
		checkPMemWrite();
	}

	void Jit::runCheckPMemWriteAndModeChange(const TWord _pc) noexcept
	{
		runCheckPMemWrite(_pc);
		checkModeChange();
	}

	void Jit::runCheckModeChange(const TWord _pc) noexcept
	{
		run(_pc);
		checkModeChange();
	}

	void Jit::runCheckLoopEnd(const TWord _pc) noexcept
	{
		run(_pc);
		checkModeChange();
		checkLoopEnd();
	}

	JitConfig Jit::getConfig(const TWord _pc) const
	{
		auto& globalConfig = getConfig();
		if(!globalConfig.getBlockConfig)
			return globalConfig;
		auto localConfig = globalConfig.getBlockConfig(_pc);
		if(localConfig)
			return *localConfig;
		return globalConfig;
	}

	void Jit::resetHW()
	{
		checkModeChange();
	}

	TJitFunc Jit::updateRunFunc(const JitCacheEntry& e)
	{
		const auto& i = e.block->getInfo();

		if(i.terminationReason == JitBlockInfo::TerminationReason::WritePMem || i.hasFlag(JitBlockInfo::Flags::WritesPMemAtLoopEnd))
		{
			if(i.hasFlag(JitBlockInfo::Flags::ModeChange))
				return &funcRunCheckPMemWriteAndModeChange;
			return &funcRunCheckPMemWrite;
		}

		// a block that may move a loop end cannot be linked to: the check has to run after it, see checkLoopEnd
		if(i.hasFlag(JitBlockInfo::Flags::WritesLA) || i.hasFlag(JitBlockInfo::Flags::LoopEndMoved))
			return &funcRunCheckLoopEnd;

		if(i.hasFlag(JitBlockInfo::Flags::ModeChange))
			return &funcRunCheckModeChange;

		if constexpr(g_traceOps)
		{
			if(e.block->getFunc())
				return &funcRun;
		}

		return e.block->getFunc();
	}

	void Jit::checkPMemWrite() noexcept
	{
		// if JIT code has written to P memory, destroy a JIT block if present at the write location
		const TWord pMemWriteAddr = m_runtimeData.m_pMemWriteAddress;

		if (pMemWriteAddr == g_pcInvalid)
			return;

		// see JitConfig::trackVolatilePMemory
		if (m_config.trackVolatilePMemory)
		{
			for (const auto& it : m_chains)
			{
				if (it.second->getBlock(pMemWriteAddr))
				{
					m_volatileP.insert(pMemWriteAddr);
					break;
				}
			}
		}

		notifyProgramMemWrite(pMemWriteAddr);
		m_dsp.notifyProgramMemWrite(pMemWriteAddr);
	}

	/*	The DSP ends a loop by comparing the address it fetches with LA, so writing LA moves the end of the running loop -
		from the loop body, a subroutine or an interrupt, further out or further in. Blocks are compiled against the loop
		registry instead, so bring the registry in line with LA for the innermost active loop.

		That loop is the topmost stack entry holding a loop body start: a DO pushes LA:LC and then its body start:SR, and
		the DO sits two words in front of the body. A long interrupt clears LF but keeps LA, so the stack rather than LF
		decides. Not modelled: the one instruction the DSP has already fetched when LA changes is still compared with the
		old value, which only matters for a write directly in front of the old or the new loop end.
	*/
	void Jit::checkLoopEnd() noexcept
	{
		const auto& regs = m_dsp.regs();

		for(auto i = static_cast<int>(m_dsp.ssIndex()); i > 0; --i)
		{
			const TWord bodyStart = hiword(regs.ss[i]).toWord();

			if(bodyStart < 2)
				continue;

			const auto it = m_loops.find(bodyStart - 2);

			if(it == m_loops.end())
				continue;

			const TWord end = (regs.la.var + 1) & 0xffffff;

			if(it->second != end)
				moveLoopEnd(it->first, end);
			return;
		}
	}

	void Jit::moveLoopEnd(const TWord _begin, const TWord _end)
	{
		const auto oldEnd = m_loops[_begin];

		if(m_loopEnds.find(_end) != m_loopEnds.end())
		{
			LOG("LA write moves the loop starting at " << HEX(_begin) << " to end at " << HEX(_end) << ", where another loop ends already, ignored");
			return;
		}

		m_loopEnds.erase(oldEnd);
		m_loopEnds.insert(_end);
		m_loops[_begin] = _end;

		// Recompile what was compiled against the old end: the block that ends there carries the loop end code, a block
		// running across the new end would carry on past it, and the DO block has to know whether its operand still matches.
		// Destroying blocks that hold a DO takes the loop out of the registry, so put it back afterwards.
		destroy(oldEnd - 1);
		destroy(_end - 1);
		destroy(_begin);

		// A destroyed single instruction block is kept for reuse, keyed by nothing but its opcode, and would come back with
		// the loop end it was compiled for. Drop those at both ends, a two word instruction starts one word earlier, and at the DO.
		for (auto& it : m_chains)
		{
			for (const TWord pc : { oldEnd - 1, oldEnd - 2, _end - 1, _end - 2, _begin })
				it.second->releaseSingleOpCache(pc);
		}

		if(m_loops.find(_begin) == m_loops.end())
			addLoop(_begin, _end);
	}

	void Jit::checkModeChange() noexcept
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

	JitBlockEmitter* Jit::acquireEmitter(JitConfig&& _config)
	{
		if(m_emitters.empty())
			return new JitBlockEmitter(dsp(), getRuntimeData(), std::move(_config));

		auto* emitter = m_emitters.back();
		m_emitters.pop_back();

		emitter->reset(std::move(_config));

		return emitter;
	}

	JitBlockEmitter* Jit::acquireEmitter(const TWord _pc)
	{
		return acquireEmitter(getConfig(_pc));
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
