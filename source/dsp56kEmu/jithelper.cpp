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

		auto regBackup = _block.dspRegPool();

		_jumpIfFalse(isFalse);

		_true();
		_block.dspRegPool().releaseAll();	// only executed if true at runtime, but always executed at compile time, reg pool now empty

		if(regBackup.hasWrittenRegs() || _hasFalseFunc)
			a.jmp(end);

		a.bind(isFalse);

		if (_hasFalseFunc)
		{
			_false();
			_block.dspRegPool().releaseAll();

			if(regBackup.hasWrittenRegs())	// do not excute the writes twice at runtime
				a.jmp(end);
		}

		regBackup.releaseAll();

		a.bind(end);
	}
}
