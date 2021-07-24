#pragma once

namespace asmjit
{
	namespace x86
	{
		class Builder;
	}
}

namespace dsp56k
{
	using JitAssembler = asmjit::x86::Builder;
}
