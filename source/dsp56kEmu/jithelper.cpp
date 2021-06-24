#include "jithelper.h"

#include "dspassert.h"
#include "jitblock.h"
#include "jitdspregs.h"
#include "logging.h"

#include "asmjit/x86/x86assembler.h"

namespace dsp56k
{
	void AsmJitErrorHandler::handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin)
	{
		LOG("Error: " << err << " - " << message);
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

	SkipLabel::SkipLabel(asmjit::x86::Assembler& _a) : m_label(_a.newLabel()), m_asm(_a)
	{
	}

	SkipLabel::~SkipLabel()
	{
		m_asm.bind(m_label);
	}

	If::If(JitBlock& _block, const std::function<void(asmjit::Label)>& _jumpIfFalse, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc)
	{
		auto& a = _block.asm_();

		const auto isFalse = a.newLabel();
		const auto end = a.newLabel();

		_block.dspRegPool().releaseAll();

		_jumpIfFalse(isFalse);

		_true();
		_block.dspRegPool().releaseAll();	// only executed if true at runtime, but always executed at compile time, reg pool now empty

		if(_hasFalseFunc)
			a.jmp(end);

		a.bind(isFalse);

		if (_hasFalseFunc)
		{
			_false();
			_block.dspRegPool().releaseAll();
		}

		a.bind(end);
	}
}
