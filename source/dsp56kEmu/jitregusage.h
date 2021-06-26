#pragma once

#include <cstdint>
#include <vector>

#include "jitregtypes.h"

namespace dsp56k
{
	class JitBlock;

	class RegUsage
	{
	public:
		using Reg = asmjit::x86::Reg;

		RegUsage(JitBlock& _block);
		~RegUsage();

		void push(const JitReg& _reg);
		void pop(const JitReg& _reg);

		void call(const void* _funcAsPtr);
		
		static bool isNonVolatile(const JitReg& _gp);
		static bool isNonVolatile(const JitReg128& _xm);
		void setUsed(const JitReg& _reg);

	private:
		JitBlock& m_block;
		uint32_t m_pushedBytes = 0;
		std::vector<Reg> m_pushedRegs;
	};
}
