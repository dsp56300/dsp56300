#include "jithelper.h"

#include "dspassert.h"
#include "jitblock.h"
#include "jitemitter.h"
#include "jitops.h"
#include "logging.h"

namespace dsp56k
{
	void AsmJitErrorHandler::handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin)
	{
		if(m_block)
		{
			LOG("Error: " << err << " - " << message << ", block at PC " << HEX(m_block->getPCFirst()) << ", P mem size " << m_block->getPMemSize() << ", disasm = " << m_block->getDisasm());
		}
		else
		{
			LOG("Error: " << err << " - " << message);
		}
		assert(false);
	}

	asmjit::Error AsmJitLogger::_log(const char* data, size_t size) noexcept
	{
		try
		{
			std::string temp(data);
			if (temp.back() == '\n')
				temp.pop_back();
			LOG(temp);
			return asmjit::kErrorOk;
		}
		catch (...)
		{
			return asmjit::kErrorInvalidArgument;
		}
	}

	SkipLabel::SkipLabel(JitEmitter& _a) : m_label(_a.newLabel()), m_asm(_a)
	{
	}

	SkipLabel::~SkipLabel()
	{
		m_asm.bind(m_label);
	}

	If::If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc, bool _updateDirtyCCR, bool _releaseRegPool)
	{
		auto& a = _block.asm_();

		auto updateDirtyCCR = [&]()
		{
			if (!_updateDirtyCCR)
				return;
			JitOps ops(_block);
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
