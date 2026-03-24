#pragma once

#include "asmjit/core/jitruntime.h"

#include <functional>
#include <string>

#include "assembler.h"
#include "unittests.h"

namespace dsp56k
{
	class JitBlock;
	class JitOps;

	class JitOptimizerTests
	{
	public:
		JitOptimizerTests();

	private:
		struct DspState
		{
			int64_t a, b;
			TWord x0, x1, y0, y1;
			TWord r[8], n[8];
			TWord sr;
		};

		// Run a DSP program both with and without the optimizer, verify results match
		void runOptimizedTest(const std::string& _name,
			const std::function<void()>& _setupDsp,
			const std::function<void(JitBlock&, JitOps&)>& _build);

		// Emit a single DSP opcode
		void emitOp(JitBlock& _block, JitOps& _ops, TWord _opA, TWord _opB = 0);

		// Assemble and emit a DSP instruction from text
		void emitAsm(JitBlock& _block, JitOps& _ops, const char* _text);

		DspState captureDspState() const;
		void compareDspState(const std::string& _name, const DspState& _a, const DspState& _b) const;

		// Test programs
		void testAccumulatorArithmetic();
		void testMACOperations();
		void testBitManipulation();
		void testSineTableGeneration();
		void testLowPassFilter();
		void testFIRFilter();
		void testBiquadFilter();
		void testDivWithImmediate();
		void testNegativeImmediate();
		void testMoveImmAdd();
		void testFullBlockPipeline();
		void testAGUOperations();

		Peripherals56362 peripheralsX;
		Peripherals56367 peripheralsY;
		Memory mem;
		DSP dsp;

		Assembler assembler;
		asmjit::JitRuntime m_rt;
	};
}
