#pragma once

#include "asmjit/core/logger.h"
#include "asmjit/core/errorhandler.h"

namespace asmjit
{
	namespace x86
	{
		class Assembler;
	}
}

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

	class SkipLabel
	{
	public:
		explicit SkipLabel(asmjit::x86::Assembler& _a);
		~SkipLabel();

		const asmjit::Label& get() const
		{
			return m_label;
		}

		operator const asmjit::Label& () const
		{
			return get();
		}

	private:
		asmjit::Label m_label;
		asmjit::x86::Assembler& m_asm;
	};
}
