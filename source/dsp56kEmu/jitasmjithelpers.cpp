#include "jitasmjithelpers.h"

#include <cassert>

#include "jitblockruntimedata.h"
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
}
