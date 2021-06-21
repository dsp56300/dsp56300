#pragma once

#include <functional>

#include "asmjit/core/logger.h"
#include "asmjit/core/errorhandler.h"
#include "asmjit/x86/x86assembler.h"

namespace asmjit
{
	namespace x86
	{
		class Assembler;
	}
}

namespace dsp56k
{
	class JitBlock;

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

	class If
	{
	public:
		If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false) : If(_block, _jumpIfFalse, _true, _false, true) {}
		If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true) : If(_block, _jumpIfFalse, _true, [](){}, false) {}
	private:
		If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc);
	};
}
