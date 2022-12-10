#pragma once

#include <functional>

#include "jittypes.h"

namespace dsp56k
{
	class DspValue;
	class JitBlock;
	class JitBlockRuntimeData;
	class JitEmitter;

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
		If(JitBlock& _block, JitBlockRuntimeData& _rt, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _updateDirtyCCR = true) : If(_block, _rt, _jumpIfFalse, _true, _false, true, _updateDirtyCCR) {}
		If(JitBlock& _block, JitBlockRuntimeData& _rt, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, bool _updateDirtyCCR = true) : If(_block, _rt, _jumpIfFalse, _true, [](){}, false, _updateDirtyCCR) {}
		If(JitBlock& _block, JitBlockRuntimeData& _rt, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc, bool _updateDirtyCCR, bool _releaseRegPool = true);
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

	JitReg64 r64(DspValue& _reg);
	JitReg32 r32(DspValue& _reg);

	JitReg64 r64(const DspValue& _reg);
	JitReg32 r32(const DspValue& _reg);
}
