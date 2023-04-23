#pragma once

#include <cstdint>
#include <map>
#include <mutex>

#include "dsp56kEmu/jitblock.h"
#include "dsp56kEmu/types.h"

namespace dsp56k
{
	struct JitBlockInfo;
}

namespace dsp56kDebugger
{
	class JitState
	{
	public:
		void onJitBlockCreated(const dsp56k::JitDspMode& _mode, const dsp56k::JitBlockRuntimeData* _block);
		void onJitBlockDestroyed(const dsp56k::JitDspMode& _mode, uint32_t _pc);

		void processChanges();

		const dsp56k::JitBlockInfo* getJitBlockInfo(dsp56k::TWord _pc) const;

	private:
		std::mutex m_mutexDspUpdate;

		// updated by DSP thread
		std::map<uint64_t, dsp56k::JitBlockInfo> m_addedBlocks;
		std::map<uint64_t, dsp56k::TWord> m_removedBlocks;

		uint64_t m_createCount = 0;
		uint64_t m_destroyCount = 0;

		// updated by GUI thread
		std::map<uint32_t, std::vector<std::shared_ptr<dsp56k::JitBlockInfo>>> m_jitBlocks;
	};
}
