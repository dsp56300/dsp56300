#pragma once

#include <cstdint>
#include <vector>

#include "jitregtypes.h"

namespace dsp56k
{
	class JitBlock;

	class JitStackHelper
	{
	public:
		JitStackHelper(JitBlock& _block);
		~JitStackHelper();

		void push(const JitRegGP& _reg);
		void push(const JitReg128& _reg);

		void pop(const JitRegGP& _reg);
		void pop(const JitReg128& _reg);
		void pop(const JitReg& _reg);
		void pop();

		void popAll();

		void pushNonVolatiles();
		
		void call(const void* _funcAsPtr) const;
		
		static bool isFuncArg(const JitRegGP& _gp);
		static bool isNonVolatile(const JitRegGP& _gp);
		static bool isNonVolatile(const JitReg128& _xm);
		void setUsed(const JitReg& _reg);
		void setUsed(const JitRegGP& _reg);
		void setUsed(const JitReg128& _reg);

		bool isUsed(const JitReg& _reg) const;
	private:
		JitBlock& m_block;
		uint32_t m_pushedBytes = 0;
		std::vector<JitReg> m_pushedRegs;
		std::vector<JitReg> m_usedRegs;
	};
}
