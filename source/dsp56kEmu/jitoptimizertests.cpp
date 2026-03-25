#include "jitoptimizertests.h"

#include "jitasmjithelpers.h"
#include "jitblock.h"
#include "jitblockruntimedata.h"
#include "jitdspmode.h"
#include "jitemitter.h"
#include "jitops.h"
#include "jitoptimizer.h"

namespace dsp56k
{
	static DefaultMemoryValidator g_optimizerTestMemValidator;

	JitOptimizerTests::JitOptimizerTests()
		: mem(g_optimizerTestMemValidator, 0x080000, 0x800000, 0x200000)
		, dsp(mem, &peripheralsX, &peripheralsY)
	{
		LOG("Running JIT Optimizer Tests...");

		testAccumulatorArithmetic();
		testMACOperations();
		testBitManipulation();
		testSineTableGeneration();
		testLowPassFilter();
		testFIRFilter();
		testBiquadFilter();
		testDivWithImmediate();
		testNegativeImmediate();
		testMoveImmAdd();
		testFullBlockPipeline();
		testAGUOperations();

		LOG("JIT Optimizer Tests finished.");
	}

	void JitOptimizerTests::runOptimizedTest(const std::string& _name,
		const std::function<void()>& _setupDsp,
		const std::function<void(JitBlock&, JitOps&)>& _build)
	{
		DspState withoutOpt{}, withOpt{};

#ifdef HAVE_ARM64
		constexpr auto arch = asmjit::Arch::kAArch64;
#else
		constexpr auto arch = asmjit::Arch::kX64;
#endif
		const auto foreignArch = m_rt.environment().arch() != arch;
		if(foreignArch)
		{
			LOG("Skipping optimizer test '" << _name << "' on foreign arch");
			return;
		}

		// Run WITHOUT optimizer
		{
			AsmJitErrorHandler errorHandler;
			asmjit::CodeHolder code;
			code.init(m_rt.environment());
			code.setErrorHandler(&errorHandler);

			JitEmitter emitter(&code);
			emitter.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);

			JitRuntimeData rtData;
			{
				JitConfig config;
				config.dynamicPeripheralAddressing = true;
				config.aguSupportBitreverse = true;

				JitBlock b(emitter, dsp, rtData, std::move(config));
				JitBlockRuntimeData rt;
				JitOps o(b, rt);

				emitter.nop();
				errorHandler.setBlock(&rt);

				PushAllUsed pusher(b);
				emitter.mov(regDspPtr, asmjit::Imm(&dsp.regs()));

				_setupDsp();
				_build(b, o);

				o.updateDirtyCCR();
				pusher.end();
			}

			emitter.ret();
			// NO optimizer
			emitter.finalize();

			TJitFunc func;
			const auto err = m_rt.add(&func, &code);
			if(err)
			{
				const auto* const errString = asmjit::DebugUtils::errorAsString(err);
				throw std::string("JIT failed (no-opt) for '") + _name + "': " + errString;
			}

			func(&dsp.regs(), 0);
			withoutOpt = captureDspState();
			m_rt.release(&func);
		}

		// Run WITH optimizer
		{
			AsmJitErrorHandler errorHandler;
			asmjit::CodeHolder code;
			code.init(m_rt.environment());
			code.setErrorHandler(&errorHandler);

			JitEmitter emitter(&code);
			emitter.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);

			JitRuntimeData rtData;
			{
				JitConfig config;
				config.dynamicPeripheralAddressing = true;
				config.aguSupportBitreverse = true;

				JitBlock b(emitter, dsp, rtData, std::move(config));
				JitBlockRuntimeData rt;
				JitOps o(b, rt);

				emitter.nop();
				errorHandler.setBlock(&rt);

				PushAllUsed pusher(b);
				emitter.mov(regDspPtr, asmjit::Imm(&dsp.regs()));

				_setupDsp();
				_build(b, o);

				o.updateDirtyCCR();
				pusher.end();
			}

			emitter.ret();

			// Apply optimizer and verify it actually changed something
			size_t optimized;
			{
				JitOptimizer optimizer(emitter);
				optimized = optimizer.optimize();
			}

			if(optimized == 0)
				throw std::string("Optimizer test '") + _name + "' FAILED: optimizer made 0 changes (expected > 0)";

			emitter.finalize();

			TJitFunc func;
			const auto err = m_rt.add(&func, &code);
			if(err)
			{
				const auto* const errString = asmjit::DebugUtils::errorAsString(err);
				throw std::string("JIT failed (with-opt) for '") + _name + "': " + errString;
			}

			func(&dsp.regs(), 0);
			withOpt = captureDspState();
			m_rt.release(&func);
		}

		compareDspState(_name, withoutOpt, withOpt);
	}

	void JitOptimizerTests::emitOp(JitBlock& _block, JitOps& _ops, TWord _opA, TWord _opB)
	{
		JitDspMode mode;
		mode.initialize(dsp);
		_block.setMode(&mode);

		_block.asm_().nop();
		_ops.emit(0, _opA, _opB);
		_block.asm_().nop();

		_block.setMode(nullptr);
	}

	void JitOptimizerTests::emitAsm(JitBlock& _block, JitOps& _ops, const char* _text)
	{
		const auto result = assembler.assemble(_text);
		if(!result.success())
			throw std::string("Assembly failed for: ") + _text;
		emitOp(_block, _ops, result.word[0], result.wordCount > 1 ? result.word[1] : 0);
	}

	JitOptimizerTests::DspState JitOptimizerTests::captureDspState() const
	{
		DspState s{};
		s.a = dsp.regs().a.var;
		s.b = dsp.regs().b.var;
		s.x0 = dsp.regs().x.var & 0xffffff;
		s.x1 = (dsp.regs().x.var >> 24) & 0xffffff;
		s.y0 = dsp.regs().y.var & 0xffffff;
		s.y1 = (dsp.regs().y.var >> 24) & 0xffffff;
		s.sr = dsp.getSR().var;
		for(int i = 0; i < 8; ++i)
		{
			s.r[i] = dsp.regs().r[i].var;
			s.n[i] = dsp.regs().n[i].var;
		}
		return s;
	}

	void JitOptimizerTests::compareDspState(const std::string& _name, const DspState& _a, const DspState& _b) const
	{
		auto check = [&](const char* _reg, int64_t _va, int64_t _vb)
		{
			if(_va != _vb)
			{
				std::stringstream ss;
				ss << "Optimizer test '" << _name << "' FAILED: " << _reg
				   << " mismatch: without_opt=0x" << std::hex << _va
				   << " with_opt=0x" << _vb;
				throw ss.str();
			}
		};

		check("a", _a.a, _b.a);
		check("b", _a.b, _b.b);
		check("x0", _a.x0, _b.x0);
		check("x1", _a.x1, _b.x1);
		check("y0", _a.y0, _b.y0);
		check("y1", _a.y1, _b.y1);
		check("sr", _a.sr, _b.sr);

		for(int i = 0; i < 8; ++i)
		{
			const std::string rName = "r" + std::to_string(i);
			const std::string nName = "n" + std::to_string(i);
			check(rName.c_str(), _a.r[i], _b.r[i]);
			check(nName.c_str(), _a.n[i], _b.n[i]);
		}

		LOG("Optimizer test '" << _name << "' PASSED");
	}

	// Test 1: Accumulator arithmetic chain - many ALU operations that produce intermediate results
	void JitOptimizerTests::testAccumulatorArithmetic()
	{
		runOptimizedTest("AccumulatorArithmetic", [&]()
		{
			dsp.resetHW();
			dsp.regs().a.var = 0x00'400000'000000;  // 0.5 in accumulator
			dsp.regs().b.var = 0x00'200000'000000;  // 0.25
			dsp.x0(0x100000);  // 0.125
			dsp.y0(0x600000);  // 0.75
		}, [&](JitBlock& block, JitOps& ops)
		{
			emitAsm(block, ops, "add b,a");       // a = 0.5 + 0.25 = 0.75
			emitAsm(block, ops, "asl a");         // a = 1.5 (saturates to ~max)
			emitAsm(block, ops, "asr a");         // a shifted back
			emitAsm(block, ops, "sub b,a");       // a = a - 0.25
			emitAsm(block, ops, "neg a");         // a = -a
			emitAsm(block, ops, "abs a");         // a = |a|
			emitAsm(block, ops, "tfr a,b");       // b = a
			emitAsm(block, ops, "add b,a");       // a = 2*a
			emitAsm(block, ops, "asr a");         // a /= 2
			emitAsm(block, ops, "sub b,a");       // a = a - b
		});
	}

	// Test 2: MAC operations - multiply-accumulate chain typical in DSP
	void JitOptimizerTests::testMACOperations()
	{
		runOptimizedTest("MACOperations", [&]()
		{
			dsp.resetHW();
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;
			dsp.x0(0x400000);  // 0.5
			dsp.x1(0x200000);  // 0.25
			dsp.y0(0x600000);  // 0.75
			dsp.y1(0x100000);  // 0.125
		}, [&](JitBlock& block, JitOps& ops)
		{
			emitAsm(block, ops, "mpy x0,y0,a");     // a = 0.5 * 0.75
			emitAsm(block, ops, "mac x1,y1,a");      // a += 0.25 * 0.125
			emitAsm(block, ops, "mac -x0,y1,a");     // a -= 0.5 * 0.125
			emitAsm(block, ops, "mpy x1,y0,b");      // b = 0.25 * 0.75
			emitAsm(block, ops, "mac x0,y0,b");       // b += 0.5 * 0.75
			emitAsm(block, ops, "add b,a");           // a += b
			emitAsm(block, ops, "rnd a");             // round a
			emitAsm(block, ops, "tfr a,b");           // b = a
		});
	}

	// Test 3: Bit manipulation operations
	void JitOptimizerTests::testBitManipulation()
	{
		runOptimizedTest("BitManipulation", [&]()
		{
			dsp.resetHW();
			dsp.regs().a.var = 0x00'555555'000000;
			dsp.regs().b.var = 0x00'aaaaaa'000000;
			dsp.x0(0x0f0f0f);
			dsp.y0(0xf0f0f0);
		}, [&](JitBlock& block, JitOps& ops)
		{
			emitAsm(block, ops, "and x0,a");     // a1 &= x0
			emitAsm(block, ops, "or y0,a");      // a1 |= y0
			emitAsm(block, ops, "eor x0,a");     // a1 ^= x0
			emitAsm(block, ops, "not a");         // a1 = ~a1
			emitAsm(block, ops, "and y0,b");     // b1 &= y0
			emitAsm(block, ops, "or x0,b");      // b1 |= x0
			emitAsm(block, ops, "lsl a");         // logical shift left a
			emitAsm(block, ops, "lsr a");         // logical shift right a
			emitAsm(block, ops, "rol a");         // rotate left
			// Load constants via long immediate (generates mov reg,imm that the
			// optimizer can propagate/fold through subsequent operations)
			emitAsm(block, ops, "move #>$0f0f0f,x0");  // reloads x0 with foldable constant
			emitAsm(block, ops, "and x0,b");     // optimizer can propagate the constant
		});
	}

	// Test 4: Sine table generation using polynomial approximation
	// Approximates sin(x) using a simple series: x - x^3/6 + x^5/120
	// Uses multiple MAC operations typical of DSP polynomial evaluation
	void JitOptimizerTests::testSineTableGeneration()
	{
		runOptimizedTest("SineTableGeneration", [&]()
		{
			dsp.resetHW();
			// x = pi/4 ~= 0.785, represented as 0.785 * 2^23 = 0x648000 (approx)
			// We'll use scaled fixed-point values
			dsp.x0(0x648000);  // x (pi/4 scaled)
			dsp.x1(0x0aa800);  // x^2/6 approximation coefficient
			dsp.y0(0x008880);  // x^4/120 approximation coefficient
			dsp.y1(0x400000);  // 0.5 (scaling factor)
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;
		}, [&](JitBlock& block, JitOps& ops)
		{
			// Polynomial evaluation for sin approximation
			emitAsm(block, ops, "mpy x0,y1,a");      // a = x * 0.5
			emitAsm(block, ops, "mpy x0,x1,b");      // b = x * (x^2/6)
			emitAsm(block, ops, "sub b,a");           // a -= x^3/6 term
			emitAsm(block, ops, "mpy x0,y0,b");      // b = x * (x^4/120)
			emitAsm(block, ops, "add b,a");           // a += x^5/120 term
			emitAsm(block, ops, "asl a");             // scale up
			emitAsm(block, ops, "rnd a");             // round result
			emitAsm(block, ops, "tfr a,b");           // save to b
			// Compute cos using sin^2 + cos^2 = 1 trick
			emitAsm(block, ops, "mpy x0,x0,a");      // a = x^2
			emitAsm(block, ops, "neg a");             // a = -x^2
			emitAsm(block, ops, "asr a");             // scale
			emitAsm(block, ops, "add b,a");           // combine
		});
	}

	// Test 5: First-order low-pass filter (24dB cascade of 4 first-order sections)
	// y[n] = y[n-1] + alpha * (x[n] - y[n-1])
	// Each section feeds into the next for 24dB/octave rolloff
	void JitOptimizerTests::testLowPassFilter()
	{
		runOptimizedTest("LowPassFilter24dB", [&]()
		{
			dsp.resetHW();
			// alpha (filter coefficient) - low cutoff
			dsp.x0(0x080000);  // alpha ~= 0.0625
			// Input sample
			dsp.x1(0x400000);  // input = 0.5
			// Previous filter states (y[n-1] for each of 4 sections)
			dsp.y0(0x100000);  // state1 = 0.125
			dsp.y1(0x080000);  // state2 = 0.0625
			// Store additional states in memory
			mem.set(MemArea_X, 0x10, 0x040000);  // state3
			mem.set(MemArea_X, 0x11, 0x020000);  // state4
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;
			dsp.regs().r[0].var = 0x10;
		}, [&](JitBlock& block, JitOps& ops)
		{
			// Section 1: y1 = y1 + alpha * (x - y1)
			emitAsm(block, ops, "tfr y0,a");         // a = y1_prev
			emitAsm(block, ops, "neg a");             // a = -y1_prev
			emitOp(block, ops, 0x54f400, 0x400000);  // move #$400000,a1 (load input into a1)
			emitAsm(block, ops, "add a,b");           // b = x - y1_prev (error term)
			emitAsm(block, ops, "mpy x0,y0,a");      // a = alpha * y0 (using y0 as proxy)
			emitAsm(block, ops, "add b,a");           // a = combined
			emitAsm(block, ops, "rnd a");
			emitAsm(block, ops, "tfr a,b");           // save section 1 output

			// Section 2: y2 = y2 + alpha * (y1_out - y2)
			emitAsm(block, ops, "mpy x0,y1,a");      // a = alpha * state2
			emitAsm(block, ops, "add b,a");           // combine with section 1 output
			emitAsm(block, ops, "rnd a");
			emitAsm(block, ops, "tfr a,b");           // section 2 output

			// Section 3 & 4 use accumulated results
			emitAsm(block, ops, "asr a");
			emitAsm(block, ops, "add b,a");
			emitAsm(block, ops, "rnd a");
		});
	}

	// Test 6: FIR filter - 8-tap FIR with coefficient multiply-accumulate
	void JitOptimizerTests::testFIRFilter()
	{
		runOptimizedTest("FIRFilter8Tap", [&]()
		{
			dsp.resetHW();
			// Set up delay line in X memory (8 samples)
			mem.set(MemArea_X, 0x20, 0x400000);  // x[0]
			mem.set(MemArea_X, 0x21, 0x350000);  // x[1]
			mem.set(MemArea_X, 0x22, 0x280000);  // x[2]
			mem.set(MemArea_X, 0x23, 0x1a0000);  // x[3]
			mem.set(MemArea_X, 0x24, 0x100000);  // x[4]
			mem.set(MemArea_X, 0x25, 0x080000);  // x[5]
			mem.set(MemArea_X, 0x26, 0x040000);  // x[6]
			mem.set(MemArea_X, 0x27, 0x020000);  // x[7]

			// Set up coefficients in Y memory (8 taps, symmetric for linear phase)
			mem.set(MemArea_Y, 0x30, 0x080000);  // h[0]
			mem.set(MemArea_Y, 0x31, 0x100000);  // h[1]
			mem.set(MemArea_Y, 0x32, 0x200000);  // h[2]
			mem.set(MemArea_Y, 0x33, 0x300000);  // h[3]
			mem.set(MemArea_Y, 0x34, 0x300000);  // h[4]
			mem.set(MemArea_Y, 0x35, 0x200000);  // h[5]
			mem.set(MemArea_Y, 0x36, 0x100000);  // h[6]
			mem.set(MemArea_Y, 0x37, 0x080000);  // h[7]

			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;
			// Load first pair of values manually
			dsp.x0(0x400000);  // x[0]
			dsp.y0(0x080000);  // h[0]
			dsp.x1(0x350000);  // x[1]
			dsp.y1(0x100000);  // h[1]
		}, [&](JitBlock& block, JitOps& ops)
		{
			// Manual MAC chain for FIR taps (unrolled inner loop)
			emitAsm(block, ops, "mpy x0,y0,a");      // a = x[0]*h[0]
			emitAsm(block, ops, "mac x1,y1,a");       // a += x[1]*h[1]

			// Load next pair and continue
			emitOp(block, ops, 0x44f400, 0x280000);   // move #$280000,x0  (x[2])
			emitOp(block, ops, 0x46f400, 0x200000);   // move #$200000,y0  (h[2])
			emitAsm(block, ops, "mac x0,y0,a");       // a += x[2]*h[2]

			emitOp(block, ops, 0x45f400, 0x1a0000);   // move #$1a0000,x1  (x[3])
			emitOp(block, ops, 0x47f400, 0x300000);   // move #$300000,y1  (h[3])
			emitAsm(block, ops, "mac x1,y1,a");       // a += x[3]*h[3]

			emitOp(block, ops, 0x44f400, 0x100000);   // move #$100000,x0  (x[4])
			emitOp(block, ops, 0x46f400, 0x300000);   // move #$300000,y0  (h[4])
			emitAsm(block, ops, "mac x0,y0,a");       // a += x[4]*h[4]

			emitOp(block, ops, 0x45f400, 0x080000);   // move #$080000,x1  (x[5])
			emitOp(block, ops, 0x47f400, 0x200000);   // move #$200000,y1  (h[5])
			emitAsm(block, ops, "mac x1,y1,a");       // a += x[5]*h[5]

			emitOp(block, ops, 0x44f400, 0x040000);   // move #$040000,x0  (x[6])
			emitOp(block, ops, 0x46f400, 0x100000);   // move #$100000,y0  (h[6])
			emitAsm(block, ops, "mac x0,y0,a");       // a += x[6]*h[6]

			emitOp(block, ops, 0x45f400, 0x020000);   // move #$020000,x1  (x[7])
			emitOp(block, ops, 0x47f400, 0x080000);   // move #$080000,y1  (h[7])
			emitAsm(block, ops, "mac x1,y1,a");       // a += x[7]*h[7]

			emitAsm(block, ops, "rnd a");              // round final result
		});
	}

	// Test 7: Biquad filter section (2nd order IIR)
	// y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
	void JitOptimizerTests::testBiquadFilter()
	{
		runOptimizedTest("BiquadFilter", [&]()
		{
			dsp.resetHW();
			// Coefficients (scaled for fixed-point)
			dsp.x0(0x200000);  // b0 = 0.25
			dsp.y0(0x400000);  // x[n] = 0.5 (input sample)
			dsp.x1(0x380000);  // b1 = 0.4375
			dsp.y1(0x300000);  // x[n-1] = 0.375

			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;

			// Store additional coefficients/state in memory
			mem.set(MemArea_X, 0x40, 0x100000);  // b2 = 0.125
			mem.set(MemArea_X, 0x41, 0x200000);  // x[n-2] = 0.25
			mem.set(MemArea_X, 0x42, 0x180000);  // a1 = 0.1875
			mem.set(MemArea_X, 0x43, 0x280000);  // y[n-1] = 0.3125
			mem.set(MemArea_X, 0x44, 0x0c0000);  // a2 = 0.09375
			mem.set(MemArea_X, 0x45, 0x1c0000);  // y[n-2] = 0.21875
		}, [&](JitBlock& block, JitOps& ops)
		{
			// b0 * x[n]
			emitAsm(block, ops, "mpy x0,y0,a");      // a = b0 * x[n]

			// b1 * x[n-1]
			emitAsm(block, ops, "mac x1,y1,a");       // a += b1 * x[n-1]

			// b2 * x[n-2] - load from memory
			emitOp(block, ops, 0x44f400, 0x100000);   // move #$100000,x0  (b2)
			emitOp(block, ops, 0x46f400, 0x200000);   // move #$200000,y0  (x[n-2])
			emitAsm(block, ops, "mac x0,y0,a");       // a += b2 * x[n-2]

			// -a1 * y[n-1]
			emitOp(block, ops, 0x44f400, 0x180000);   // move #$180000,x0  (a1)
			emitOp(block, ops, 0x46f400, 0x280000);   // move #$280000,y0  (y[n-1])
			emitAsm(block, ops, "mac -x0,y0,a");      // a -= a1 * y[n-1]

			// -a2 * y[n-2]
			emitOp(block, ops, 0x44f400, 0x0c0000);   // move #$0c0000,x0  (a2)
			emitOp(block, ops, 0x46f400, 0x1c0000);   // move #$1c0000,y0  (y[n-2])
			emitAsm(block, ops, "mac -x0,y0,a");      // a -= a2 * y[n-2]

			emitAsm(block, ops, "rnd a");              // round
			emitAsm(block, ops, "tfr a,b");            // save output
		});
	}

	void JitOptimizerTests::testDivWithImmediate()
	{
		runOptimizedTest("DivWithImmediate", [&]()
		{
			dsp.resetHW();
			dsp.regs().a.var = 0x00'200000'000000;
			dsp.regs().b.var = 0;
			dsp.y1(0);
		}, [&](JitBlock& block, JitOps& ops)
		{
			emitAsm(block, ops, "move #>$400,y1");
			emitAsm(block, ops, "div y1,a");
			emitAsm(block, ops, "div y1,a");
			emitAsm(block, ops, "div y1,a");
			emitAsm(block, ops, "div y1,a");
			emitAsm(block, ops, "div y1,a");
			emitAsm(block, ops, "div y1,a");
			emitAsm(block, ops, "tfr a,b");
		});
	}

	void JitOptimizerTests::testNegativeImmediate()
	{
		runOptimizedTest("NegativeImmediate", [&]()
		{
			dsp.resetHW();
			dsp.regs().a.var = 0x00'400000'000000;
			dsp.regs().b.var = 0;
			dsp.x0(0x800000);
			dsp.y0(0xFF0000);
		}, [&](JitBlock& block, JitOps& ops)
		{
			emitAsm(block, ops, "mpy x0,y0,b");
			emitAsm(block, ops, "add b,a");
			emitAsm(block, ops, "neg a");
			emitAsm(block, ops, "asr a");
			emitAsm(block, ops, "and x0,a");
			emitAsm(block, ops, "or y0,a");
			emitAsm(block, ops, "not a");
			emitAsm(block, ops, "tfr a,b");
		});
	}

	// Test: move #>imm,y1 + add y1,b - the exact VTune hotspot pattern
	void JitOptimizerTests::testMoveImmAdd()
	{
		runOptimizedTest("MoveImmAdd", [&]()
		{
			dsp.resetHW();
			dsp.regs().a.var = 0x00'200000'000000;
			dsp.regs().b.var = 0x00'100000'000000;
			dsp.y1(0);
		}, [&](JitBlock& block, JitOps& ops)
		{
			emitAsm(block, ops, "move #>$400,y1");
			emitAsm(block, ops, "add y1,b");
		});
	}

	void JitOptimizerTests::testAGUOperations()
	{
		runOptimizedTest("AGUOperations", [&]()
		{
			dsp.resetHW();
			dsp.regs().r[0].var = 0x100;
			dsp.regs().r[1].var = 0x200;
			dsp.regs().r[2].var = 0x300;
			dsp.regs().n[0].var = 0x10;
			dsp.regs().n[1].var = 0x20;
			dsp.regs().n[2].var = 0x30;
			dsp.set_m(0, 0xffffff);  // linear
			dsp.set_m(1, 0xffffff);  // linear
			dsp.set_m(2, 0xffffff);  // linear

			dsp.regs().a.var = 0x00'400000'000000;
			dsp.regs().b.var = 0x00'200000'000000;
			dsp.x0(0x100000);
			dsp.y0(0x300000);
		}, [&](JitBlock& block, JitOps& ops)
		{
			// Various ALU ops interleaved with register loads to stress the optimizer
			emitAsm(block, ops, "add b,a");
			emitAsm(block, ops, "asl a");
			emitAsm(block, ops, "sub b,a");
			emitAsm(block, ops, "mpy x0,y0,b");
			emitAsm(block, ops, "add b,a");
			emitAsm(block, ops, "neg b");
			emitAsm(block, ops, "mac x0,y0,a");
			emitAsm(block, ops, "rnd a");
			emitAsm(block, ops, "rnd b");
			emitAsm(block, ops, "asr a");
			emitAsm(block, ops, "asr b");
			emitAsm(block, ops, "add b,a");
			emitAsm(block, ops, "tfr a,b");
		});
	}

	// Test using the FULL JitBlock::emit pipeline — writes DSP opcodes into
	// P memory and compiles with the real block infrastructure + optimizer.
	// This matches exactly what happens in the actual DSP emulation.
	void JitOptimizerTests::testFullBlockPipeline()
	{
		const std::string name = "FullBlockPipeline";
		LOG("Optimizer test '" << name << "' starting...");

		// Assemble DSP program into P memory
		const TWord basePC = 0x100;
		TWord pc = basePC;

		auto writeAsm = [&](const char* _text)
		{
			const auto result = assembler.assemble(_text);
			if(!result.success())
				throw std::string("Assembly failed for: ") + _text;
			mem.set(MemArea_P, pc, result.word[0]);
			if(result.wordCount > 1)
				mem.set(MemArea_P, pc + 1, result.word[1]);
			pc += result.wordCount;
		};

		// The VTune hotspot block
		writeAsm("move #>$400,y1");
		writeAsm("add y1,b");
		writeAsm("sub b,a");
		writeAsm("asr #$2,a,a");
		writeAsm("rts");

		// Set up DSP state
		dsp.resetHW();
		dsp.regs().a.var = 0x00'300000'000000;
		dsp.regs().b.var = 0x00'100000'000000;
		dsp.y1(0);
		dsp.set_m(0, 0xffffff);
		// Push a return address on the stack so rts has somewhere to go
		dsp.regs().sp.var = 0;
		dsp.regs().ss[0].var = (static_cast<uint64_t>(0x200) << 24);  // SSH = return PC

		// Run WITHOUT optimizer
		DspState withoutOpt{};
		{
			AsmJitErrorHandler errorHandler;
			asmjit::CodeHolder code;
			code.init(m_rt.environment());
			code.setErrorHandler(&errorHandler);

			JitEmitter emitter(&code);

			JitRuntimeData rtData;
			JitConfig config;
			config.dynamicPeripheralAddressing = true;
			config.aguSupportBitreverse = true;
			config.enableOptimizer = false;

			JitBlock block(emitter, dsp, rtData, std::move(config));
			JitBlockRuntimeData rt;

			const std::vector<JitCacheEntry> cache;
			const std::set<TWord> volatileP;
			const std::map<TWord, TWord> loopStarts;
			const std::set<TWord> loopEnds;

			errorHandler.setBlock(&rt);
			block.emit(rt, nullptr, basePC, cache, volatileP, loopStarts, loopEnds, false);

			emitter.finalize();

			TJitFunc func;
			const auto err = m_rt.add(&func, &code);
			if(err)
				throw std::string("JIT failed (no-opt) for '") + name + "'";

			dsp.resetHW();
			dsp.regs().a.var = 0x00'300000'000000;
			dsp.regs().b.var = 0x00'100000'000000;
			dsp.y1(0);
			dsp.set_m(0, 0xffffff);
			dsp.regs().sp.var = 0;
			dsp.regs().ss[0].var = (static_cast<uint64_t>(0x200) << 24);

			func(&dsp.regs(), 0);
			withoutOpt = captureDspState();
			m_rt.release(&func);
		}

		// Run WITH optimizer
		DspState withOpt{};
		{
			AsmJitErrorHandler errorHandler;
			asmjit::CodeHolder code;
			code.init(m_rt.environment());
			code.setErrorHandler(&errorHandler);



			JitEmitter emitter(&code);

			JitRuntimeData rtData;
			JitConfig config;
			config.dynamicPeripheralAddressing = true;
			config.aguSupportBitreverse = true;
			config.enableOptimizer = true;

			JitBlock block(emitter, dsp, rtData, std::move(config));
			JitBlockRuntimeData rt;

			const std::vector<JitCacheEntry> cache;
			const std::set<TWord> volatileP;
			const std::map<TWord, TWord> loopStarts;
			const std::set<TWord> loopEnds;

			errorHandler.setBlock(&rt);
			block.emit(rt, nullptr, basePC, cache, volatileP, loopStarts, loopEnds, false);

			// Apply optimizer and verify it actually changed something
			size_t optimized;
			{
				JitOptimizer optimizer(emitter);
				optimized = optimizer.optimize();
			}

			if(optimized == 0)
				throw std::string("Optimizer test '") + name + "' FAILED: optimizer made 0 changes (expected > 0)";

			emitter.finalize();

			TJitFunc func;
			const auto err = m_rt.add(&func, &code);
			if(err)
				throw std::string("JIT failed (with-opt) for '") + name + "'";

			dsp.resetHW();
			dsp.regs().a.var = 0x00'300000'000000;
			dsp.regs().b.var = 0x00'100000'000000;
			dsp.y1(0);
			dsp.set_m(0, 0xffffff);
			dsp.regs().sp.var = 0;
			dsp.regs().ss[0].var = (static_cast<uint64_t>(0x200) << 24);

			func(&dsp.regs(), 0);
			withOpt = captureDspState();
			m_rt.release(&func);
		}

		compareDspState(name, withoutOpt, withOpt);
	}
}
