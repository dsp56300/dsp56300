#pragma once

#include <functional>

#include "jittypes.h"

#include "asmjit/core/logger.h"
#include "asmjit/core/errorhandler.h"

namespace dsp56k
{
	class JitBlock;
	class JitEmitter;

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
		explicit SkipLabel(JitEmitter& _a);
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
		JitEmitter& m_asm;
	};

	class If
	{
	public:
		If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false) : If(_block, _jumpIfFalse, _true, _false, true) {}
		If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true) : If(_block, _jumpIfFalse, _true, [](){}, false) {}
	private:
		If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc);
		void updateDirtyCCR(JitBlock& _block);
	};

	static JitReg32 r32(const JitRegGP& _reg)
	{
#ifdef HAVE_ARM64
		return _reg.w();
#else
		return _reg.r32();
#endif
	}

	static JitReg64 r64(const JitRegGP& _reg)
	{
#ifdef HAVE_ARM64
		return _reg.x();
#else
		return _reg.r64();
#endif
	}

	static auto shiftOperand(const JitRegGP& _reg)
	{
#ifdef HAVE_ARM64
		return _reg;
#else
		return _reg.r8();
#endif
	}
}
