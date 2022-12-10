#pragma once

#include "jitblockruntimedata.h"

namespace dsp56k
{
	class JitProfilingSupport
	{
	public:
		JitProfilingSupport(const DSP& _dsp);
		~JitProfilingSupport();

		static bool isBeingProfiled();
		void addJitBlock(JitBlockRuntimeData& _jitBlock);

	private:
		struct FileInfo
		{
			std::string name;
			std::vector<JitBlockRuntimeData::InstructionProfilingInfo> info;
		};

		void threadWriteSources();

		const DSP& m_dsp;

		std::map<TWord, uint32_t> m_methodCountsPerPC;
		RingBuffer<FileInfo, 128, true> m_outQueue;

		std::unique_ptr<std::thread> m_fileWriter;
		std::string m_rootPath;
	};
}
