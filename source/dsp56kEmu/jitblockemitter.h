#pragma once

#include "jitblock.h"
#include "jitemitter.h"

#include "asmjit/core/codeholder.h"

namespace dsp56k
{
	struct JitConfig;
	struct JitRuntimeData;
	class DSP;

	struct JitBlockEmitter
	{
		JitBlockEmitter(DSP& _dsp, JitRuntimeData& _runtimeData, JitConfig&& _config)
		: emitter(nullptr)
		, block(emitter, _dsp, _runtimeData, std::move(_config))
		{
		}

		void reset(JitConfig&& _config)
		{
			emitter.~JitEmitter();
			codeHolder.~CodeHolder();

			new (&codeHolder)asmjit::CodeHolder();
			new (&emitter)JitEmitter(nullptr);

			block.reset(std::move(_config));
		}

		asmjit::CodeHolder codeHolder;
		JitEmitter emitter;
		JitBlock block;
	};
}
