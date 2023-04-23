#include "debuggerJitState.h"

#include "dsp56kEmu/jitdspmode.h"
#include "dsp56kEmu/jitblockruntimedata.h"

namespace dsp56kDebugger
{
	void fromKey(uint32_t& _mode, dsp56k::TWord& _pc, uint64_t _key)
	{
		_pc = _key & 0xffffffff;
		_mode = _key >> 32ull;
	}

	uint64_t makeKey(const dsp56k::JitDspMode& _mode, const dsp56k::TWord _pc)
	{
		uint64_t r = _mode.get();
		r <<= 32ull;
		r |= _pc;
		return r;
	}

	void JitState::onJitBlockCreated(const dsp56k::JitDspMode& _mode, const dsp56k::JitBlockRuntimeData* _block)
	{
		const auto& info = _block->getInfo();
		const auto k = makeKey(_mode, info.pc);

		std::lock_guard lock(m_mutexDspUpdate);
		m_addedBlocks[k] = info;
		m_removedBlocks.erase(k);
		++m_createCount;
	}

	void JitState::onJitBlockDestroyed(const dsp56k::JitDspMode& _mode, uint32_t _pc)
	{
		const auto k = makeKey(_mode, _pc);

		std::lock_guard lock(m_mutexDspUpdate);
		m_removedBlocks[k] = _pc;
		m_addedBlocks.erase(k);
		++m_destroyCount;
	}

	void JitState::processChanges()
	{
		std::map<uint64_t, dsp56k::JitBlockInfo> addedBlocks;
		std::map<uint64_t, dsp56k::TWord> removedBlocks;

		uint64_t createCount;
		uint64_t destroyCount;

		{
			std::lock_guard l(m_mutexDspUpdate);

			std::swap(addedBlocks, m_addedBlocks);
			std::swap(removedBlocks, m_removedBlocks);

			createCount = m_createCount;
			destroyCount = m_destroyCount;
			m_createCount = m_destroyCount = 0;
		}

		for (const auto& removedBlock : removedBlocks)
		{
			uint32_t mode;
			dsp56k::TWord pc;
			fromKey(mode, pc, removedBlock.first);

			auto itModeBlocks = m_jitBlocks.find(mode);

			if(itModeBlocks == m_jitBlocks.end())
				continue;

			auto& blocksPerMode = itModeBlocks->second;

			if(pc >= blocksPerMode.size())
				continue;
			const auto block = blocksPerMode[pc];
			if(!block)
				continue;
			
			for(auto i = block->pc; i < block->pc + block->memSize; ++i)
				blocksPerMode[i].reset();
		}

		for(const auto& addedBlock : addedBlocks)
		{
			uint32_t mode;
			dsp56k::TWord pc;
			fromKey(mode, pc, addedBlock.first);

			const auto& jitBlockInfo = addedBlock.second;

			auto blockInfo = std::shared_ptr<dsp56k::JitBlockInfo>();
			blockInfo.reset(new dsp56k::JitBlockInfo(jitBlockInfo));

			auto itModeBlocks = m_jitBlocks.find(mode);

			if(itModeBlocks == m_jitBlocks.end())
			{
				std::vector<std::shared_ptr<dsp56k::JitBlockInfo>> newMap;
				newMap.resize(jitBlockInfo.pc + jitBlockInfo.memSize);
				for(auto p = jitBlockInfo.pc; p < jitBlockInfo.pc + jitBlockInfo.memSize; ++p)
					newMap[p] = blockInfo;

				m_jitBlocks.insert(std::make_pair(mode, newMap));
			}
			else
			{
				auto& map = itModeBlocks->second;
				if(map.size() < jitBlockInfo.pc + jitBlockInfo.memSize)
					map.resize(jitBlockInfo.pc + jitBlockInfo.memSize);
				for(auto p = jitBlockInfo.pc; p < jitBlockInfo.pc + jitBlockInfo.memSize; ++p)
					map[p] = blockInfo;
			}
		}
	}
	const dsp56k::JitBlockInfo* JitState::getJitBlockInfo(dsp56k::TWord _pc) const
	{
		for (const auto& it : m_jitBlocks)
		{
			const auto& vec = it.second;
			if(_pc >= vec.size())
				continue;
			const auto ptr = vec[_pc];
			if(ptr)
				return ptr.get();
		}
		return nullptr;
	}
}
