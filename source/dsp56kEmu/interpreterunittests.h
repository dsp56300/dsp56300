#pragma once

#include "unittests.h"

namespace dsp56k
{
	class InterpreterUnitTests : public UnitTests
	{
	public:
		InterpreterUnitTests();
	private:
		void testSubr();
		void testCCCC();
		void testCCCC(int64_t _val, int64_t _compareValue, bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq);

		void runTest(const std::function<void()>& _build, const std::function<void()>& _verify) override;
		void emit(TWord _opA, TWord _opB = 0, TWord _pc = 0) override;

		void execOpcode(uint32_t _op0, uint32_t _op1 = 0, bool _reset=false, TWord _pc = 0);
	};
}
