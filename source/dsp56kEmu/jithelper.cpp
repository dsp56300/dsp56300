#include "jithelper.h"

#include "assert.h"
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

	SkipLabel::SkipLabel(asmjit::x86::Assembler& _a): m_asm(_a), m_label(_a.newLabel())
	{
	}

	SkipLabel::~SkipLabel()
	{
		m_asm.bind(m_label);
	}
}
