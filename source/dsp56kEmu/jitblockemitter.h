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
		JitBlockEmitter(DSP& _dsp, JitRuntimeData& _runtimeData, const JitConfig& _config)
		: emitter(nullptr)
		, block(emitter, _dsp, _runtimeData, _config)
		{
		}

		void reset()
		{
			emitter.~JitEmitter();
			codeHolder.~CodeHolder();

			new (&codeHolder)asmjit::CodeHolder();
			new (&emitter)JitEmitter(nullptr);

			block.reset();
		}

		asmjit::CodeHolder codeHolder;
		JitEmitter emitter;
		JitBlock block;
	};
}
