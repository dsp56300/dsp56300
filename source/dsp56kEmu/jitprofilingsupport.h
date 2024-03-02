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
#ifdef DSP56K_USE_PERF_JIT_PROFILING
		struct PerfSymbolInfo
		{
			int pid;
			uint64_t startAdr;
			uint64_t codeSize;
			std::string symbolName;
			std::string sourceFile;
		};
#endif

		void threadWriteSources();

#ifdef DSP56K_USE_PERF_JIT_PROFILING
		void threadWritePerfSymbols();
#endif
		const DSP& m_dsp;

		std::map<TWord, uint32_t> m_methodCountsPerPC;
		RingBuffer<FileInfo, 128, true> m_outQueue;

#ifdef DSP56K_USE_PERF_JIT_PROFILING
		RingBuffer<PerfSymbolInfo, 16384, true> m_symbolQueue;
#endif
		std::unique_ptr<std::thread> m_fileWriter;
#ifdef DSP56K_USE_PERF_JIT_PROFILING
		std::unique_ptr<std::thread> m_symbolWriter;
#endif
		std::string m_rootPath;
	};
}
