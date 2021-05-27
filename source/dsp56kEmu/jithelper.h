#pragma once

#include "asmjit/core/logger.h"
#include "asmjit/core/errorhandler.h"

namespace dsp56k
{
	class AsmJitErrorHandler final : public asmjit::ErrorHandler
	{
		void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override;
	};

	class AsmJitLogger final : public asmjit::Logger
	{
		asmjit::Error _log(const char* data, size_t size) noexcept override;
	};
}
