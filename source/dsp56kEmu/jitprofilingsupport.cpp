#include "jitprofilingsupport.h"

#include <fstream>

#include "disasm.h"
#include "dsp.h"
#include "threadtools.h"

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
#include "../vtuneSdk/include/jitprofiling.h"
#endif

#ifdef DSP56K_USE_PERF_JIT_PROFILING
#include <unistd.h>		// getpid()
#include <sys/stat.h>	// mkdir
#endif

#if defined(_WIN32)
#include <filesystem>
#endif

namespace dsp56k
{
	JitProfilingSupport::JitProfilingSupport(const DSP& _dsp) : m_dsp(_dsp)
	{
#ifdef _WIN32
		auto dir = std::filesystem::current_path();
		dir = dir.append("profiling_src");
		std::filesystem::create_directory(dir);
		m_rootPath = dir.string();
#elif defined(DSP56K_USE_PERF_JIT_PROFILING)
		m_rootPath = "/tmp/dsp56300_profiling_src";
		mkdir("/tmp", 0777);
		mkdir(m_rootPath.c_str(), 0777);
#endif

		m_fileWriter.reset(new std::thread([this]()
		{
			dsp56k::ThreadTools::setCurrentThreadName("jitSourceWriter");
			threadWriteSources();
		}));

#ifdef DSP56K_USE_PERF_JIT_PROFILING
		m_symbolWriter.reset(new std::thread([this]() {
			dsp56k::ThreadTools::setCurrentThreadName("perfSymbolWriter");
			threadWritePerfSymbols();
		}));
#endif
	}

	JitProfilingSupport::~JitProfilingSupport()
	{
		// write empty file info to terminate thread
		const FileInfo fi{};
		m_outQueue.push_back(fi);

		m_fileWriter->join();
		m_fileWriter.reset();

#ifdef DSP56K_USE_PERF_JIT_PROFILING
		PerfSymbolInfo si{};
		m_symbolQueue.push_back(si);

		m_symbolWriter->join();
		m_symbolWriter.reset();
#endif
	}

	bool JitProfilingSupport::isBeingProfiled()
	{
#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
		if(iJIT_IsProfilingActive() != iJIT_NOTHING_RUNNING)
			return true;
#endif
		return false;	// FIXME: Can we determine if we are being profiled with perf? Set to true manually for now if needed
	}

	void JitProfilingSupport::addJitBlock(JitBlockRuntimeData& b)
	{
		const auto itMethod = m_methodCountsPerPC.find(b.getPCFirst());

		uint32_t uid;
		if (itMethod == m_methodCountsPerPC.end())
		{
			m_methodCountsPerPC.insert(std::make_pair(b.getPCFirst(), 1));
			uid = 1;
		}
		else
		{
			uid = itMethod->second;
			++itMethod->second;
		}

		char methodName[64];
		sprintf(methodName, "%06x-%06x_%x", b.getPCFirst(), b.getPCFirst() + b.getPMemSize() - 1, uid);

		if (b.getInfo().terminationReason == JitBlockInfo::TerminationReason::LoopEnd)
			strcat(methodName, "_L");
		if (b.getInfo().terminationReason == JitBlockInfo::TerminationReason::WritePMem)
			strcat(methodName, "_P");

		const std::string filename = m_rootPath + '/' + methodName + ".asm";

		std::vector<JitBlockRuntimeData::InstructionProfilingInfo> pis;
		pis.swap(b.getProfilingInfo());

#ifdef DSP56K_USE_VTUNE_JIT_PROFILING_API
		iJIT_Method_Load jmethod = {};
		jmethod.method_id = iJIT_GetNewMethodID();
		jmethod.method_name = methodName;
		jmethod.class_file_name = const_cast<char*>("dsp56k::Jit");
		jmethod.source_file_name = const_cast<char*>(filename.c_str());
		jmethod.method_load_address = reinterpret_cast<void*>(b.getFunc());
		jmethod.method_size = static_cast<unsigned int>(b.getCodeSize());

		std::vector<LineNumberInfo> lineNumberInfos;
		lineNumberInfos.reserve(pis.size() + 2);

		uint32_t lineNumber = 1;
		lineNumberInfos.emplace_back(LineNumberInfo{static_cast<uint32_t>(pis.front().codeOffset), lineNumber++});

		for (const auto& pi : pis)
		{
			lineNumberInfos.emplace_back(LineNumberInfo{static_cast<uint32_t>(pi.codeOffsetAfter), lineNumber});
			lineNumber += pi.lineCount;
		}

		lineNumberInfos.emplace_back(LineNumberInfo{static_cast<uint32_t>(pis.back().codeOffsetAfter), lineNumber});

		jmethod.line_number_size = static_cast<uint32_t>(lineNumberInfos.size());
		jmethod.line_number_table = &lineNumberInfos.front();

		iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &jmethod);
#endif

#ifdef DSP56K_USE_PERF_JIT_PROFILING
		PerfSymbolInfo si;
		si.pid = getpid();
		si.startAdr = reinterpret_cast<uint64_t>(b.getFunc());
		si.codeSize = b.getCodeSize();
		si.symbolName = methodName;
		si.sourceFile = filename;

		m_symbolQueue.push_back(si);
#endif

		FileInfo fi{};
		fi.info.swap(pis);
		fi.name = filename;
		m_outQueue.push_back(fi);
	}

	void JitProfilingSupport::threadWriteSources()
	{
		const Opcodes opcodes;
		Disassembler disasm(opcodes);

		disasm.addSymbols(m_dsp.memory());

		m_dsp.getPeriph(0)->setSymbols(disasm);
		m_dsp.getPeriph(1)->setSymbols(disasm);

		while(true)
		{
			const auto fi = m_outQueue.pop_front();
			if (fi.name.empty())
				return;

			std::ofstream of(fi.name.c_str(), std::ios::out);

			if (!of.is_open())
				return;

			of << "<prolog>" << std::endl;

			const auto& pis = fi.info;

			std::string d;

			for (const auto & pi : pis)
			{
				if(!pi.sourceText.empty())
				{
					of << '<' << pi.sourceText << '>' << std::endl;
				}
				else
				{
					disasm.disassemble(d, pi.opA, pi.opB, 0, 0, pi.pc);
					of << d << std::endl;
				}

				if(pi.lineCount == 2)
				{
					// pi is rep, emit repeated instruction also
					disasm.disassemble(d, pi.opB, 0, 0, 0, pi.pc + 1);
					of << d << std::endl;
				}
			}

			of << "<epilog>" << std::endl;

			of.close();
		}
	}

#ifdef DSP56K_USE_PERF_JIT_PROFILING
	void JitProfilingSupport::threadWritePerfSymbols()
	{
		FILE *fp = nullptr;

		while (true)
		{
			const auto si = m_symbolQueue.pop_front();

			if(si.symbolName.empty())
			{
				if (fp)
				{
					fclose(fp);
					fp = nullptr;
				}
				break;
			}

			if (!fp)
			{
				char symbolFileName[64];
				snprintf(symbolFileName, sizeof(symbolFileName), "/tmp/perf-%d.map", si.pid);
				fp = fopen(symbolFileName, "a+");

				if (!fp)
				{
					fprintf(stderr, "can't open %s\n", symbolFileName);
					continue;
				}
			}

			fprintf(fp, "%lx %lx %s\n", si.startAdr, si.codeSize, si.symbolName.c_str());
//			fprintf(fp, "%lx %lx %s %s:1\n", si.startAdr, si.codeSize, si.symbolName.c_str(), si.sourceFile.c_str());
			if (m_symbolQueue.empty())
			{
				fclose(fp);
				fp = nullptr;
			}
		}
	}
#endif
}
