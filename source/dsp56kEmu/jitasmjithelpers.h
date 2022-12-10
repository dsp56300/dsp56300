#pragma once

#include "asmjit/core/logger.h"
#include "asmjit/core/errorhandler.h"

namespace dsp56k
{
	class JitBlockRuntimeData;

	class AsmJitErrorHandler final : public asmjit::ErrorHandler
	{
	public:
		void setBlock(const JitBlockRuntimeData* _b) { m_block = _b; }
	private:
		void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override;
		const JitBlockRuntimeData* m_block = nullptr;
	};

	class AsmJitLogger final : public asmjit::Logger
	{
		asmjit::Error _log(const char* data, size_t size) noexcept override;
	};
}