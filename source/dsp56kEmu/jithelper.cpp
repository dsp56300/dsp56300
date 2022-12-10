#include "jithelper.h"

#include "jitblock.h"
#include "jitblockruntimedata.h"
#include "jitemitter.h"
#include "jitops.h"

namespace dsp56k
{
	SkipLabel::SkipLabel(JitEmitter& _a) : m_label(_a.newLabel()), m_asm(_a)
	{
	}

	SkipLabel::~SkipLabel()
	{
		m_asm.bind(m_label);
	}

	If::If(JitBlock& _block, JitBlockRuntimeData& _rt, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc, bool _updateDirtyCCR, bool _releaseRegPool)
	{
		auto& a = _block.asm_();

		auto updateDirtyCCR = [&]()
		{
			if (!_updateDirtyCCR)
				return;
			JitOps ops(_block, _rt);
			ops.updateDirtyCCR();
		};

		auto execIf = [&](bool _pushNonVolatiles, PushMover& _pm)
		{
			const auto isFalse = a.newLabel();
			const auto end = a.newLabel();

			updateDirtyCCR();

			if(_releaseRegPool)
				_block.dspRegPool().releaseNonLocked();

			if(_pushNonVolatiles)
				_block.stack().pushNonVolatiles();

			_pm.begin();

			_jumpIfFalse(isFalse);

			{
				_true();

				updateDirtyCCR();

				if(_releaseRegPool)
					_block.dspRegPool().releaseNonLocked();	// only executed if true at runtime, but always executed at compile time, reg pool now empty
			}

			if (_hasFalseFunc)
				a.jmp(end);

			a.bind(isFalse);

			if (_hasFalseFunc)
			{
				_false();

				updateDirtyCCR();

				if(_releaseRegPool)
					_block.dspRegPool().releaseNonLocked();
			}

			a.bind(end);
		};

		// we move all register pushed that happened inside the branches to the outside to make sure they are always executed
		PushMover pm(_block, false);
		execIf(false, pm);
	}

	JitReg64 r64(DspValue& _reg)
	{
		return r64(_reg.get());
	}

	JitReg32 r32(DspValue& _reg)
	{
		return r32(_reg.get());
	}

	JitReg64 r64(const DspValue& _reg)
	{
		return r64(_reg.get());
	}

	JitReg32 r32(const DspValue& _reg)
	{
		return r32(_reg.get());
	}
}
