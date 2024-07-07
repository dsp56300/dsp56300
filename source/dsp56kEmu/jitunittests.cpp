#include "jitunittests.h"

#include "jitasmjithelpers.h"
#include "jitblock.h"
#include "jitblockruntimedata.h"
#include "jitemitter.h"
#include "jithelper.h"
#include "jitops.h"

namespace dsp56k
{
	static constexpr bool g_useDspMode = true;

	JitUnittests::JitUnittests(bool _logging/* = true*/)
	: m_checks({})
	, m_logging(_logging)
	{
		runTest(&JitUnittests::conversion_build, &JitUnittests::conversion_verify);
		runTest(&JitUnittests::signextend_build, &JitUnittests::signextend_verify);

		runTest(&JitUnittests::ccr_u_build, &JitUnittests::ccr_u_verify);
		runTest(&JitUnittests::ccr_e_build, &JitUnittests::ccr_e_verify);
		runTest(&JitUnittests::ccr_n_build, &JitUnittests::ccr_n_verify);
		runTest(&JitUnittests::ccr_s_build, &JitUnittests::ccr_s_verify);

		runTest(&JitUnittests::agu_build, &JitUnittests::agu_verify);
		runTest(&JitUnittests::agu_modulo_build, &JitUnittests::agu_modulo_verify);
		runTest(&JitUnittests::agu_modulo2_build, &JitUnittests::agu_modulo2_verify);

		runTest(&JitUnittests::transferSaturation_build, &JitUnittests::transferSaturation_verify);
		transferSaturation48();

		{
			constexpr auto T=true;
			constexpr auto F=false;

			//                            <  <= =  >= >  != 
			testCCCC(0xff000000000000, 0, T, T, F, F, F, T);
			testCCCC(0x00ff0000000000, 0, F, F, F, T, T, T);
			testCCCC(0x00000000000000, 0, F, T, T, T ,F ,F);
		}

		decode_dddddd_write();
		decode_dddddd_read();

		runTest(&JitUnittests::getSS_build, &JitUnittests::getSS_verify);
		runTest(&JitUnittests::getSetRegs_build, &JitUnittests::getSetRegs_verify);

		runAllTests();

		jitDiv();
		rep_div();

		parallelMoveXY();
	}

	JitUnittests::~JitUnittests()
	{
		m_rt.reset(asmjit::ResetPolicy::kHard);
	}

	void JitUnittests::runTest(void( JitUnittests::* _build)(), void( JitUnittests::* _verify)())
	{
		runTest([&]()
		{
			(this->*_build)();
		},[&]()
		{
			(this->*_verify)();
		});
	}

	void JitUnittests::runTest(const std::function<void()>& _build, const std::function<void()>& _verify)
	{
		AsmJitErrorHandler errorHandler;
		asmjit::CodeHolder code;
		AsmJitLogger logger;
		logger.addFlags(asmjit::FormatFlags::kHexImms | /*asmjit::FormatFlags::kHexOffsets |*/ asmjit::FormatFlags::kMachineCode);

#ifdef HAVE_ARM64
		constexpr auto arch = asmjit::Arch::kAArch64;
#else
		constexpr auto arch = asmjit::Arch::kX64;
#endif

		const auto foreignArch = m_rt.environment().arch() != arch;

		if(foreignArch)
			code.init(asmjit::Environment(arch));
		else
			code.init(m_rt.environment());

		if(m_logging)
			code.setLogger(&logger);
		code.setErrorHandler(&errorHandler);

		JitEmitter m_asm(&code);

		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
		m_asm.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateAssembler);

		if(m_logging)
			LOG("Creating test code");

		JitRuntimeData rtData;

		{
			JitConfig config;
			config.dynamicPeripheralAddressing = true;
			config.aguSupportBitreverse = true;

			JitBlock b(m_asm, dsp, rtData, std::move(config));
			JitBlockRuntimeData rt;
			JitOps o(b, rt);

			block = &b;
			ops = &o;

			m_asm.nop();

			errorHandler.setBlock(&rt);

			PushAllUsed pusher(b);

			m_asm.mov(regDspPtr, asmjit::Imm(&dsp.regs()));
			_build();

			block = nullptr;
			ops = nullptr;

			o.updateDirtyCCR();

			pusher.end();
		}

		m_asm.ret();

		m_asm.finalize();

		TJitFunc func;
		const auto err = m_rt.add(&func, &code);
		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			std::stringstream ss;
			ss << "JIT failed: " << err << " - " << errString;
			const std::string msg(ss.str());
			LOG(msg);
			throw std::runtime_error(msg);
		}

		if(!foreignArch)
		{
			if(m_logging)
				LOG("Running test code");

			func(&dsp.getJit(), 0xbadbc);

			if(m_logging)
				LOG("Verifying test code");

			_verify();
		}
		else
		{
			LOG("Run & Verify of code for foreign arch skipped");
		}

		m_rt.release(&func);
	}

	void JitUnittests::nop(size_t _count) const
	{
		for(size_t i=0; i<_count; ++i)
			block->asm_().nop();
	}

	void JitUnittests::conversion_build()
	{
		block->asm_().bind(block->asm_().newNamedLabel("test_conv"));

		dsp.regs().x.var = 0xffeedd112233;
		dsp.regs().y.var = 0x445566fedcba;

		const RegGP r(*block);

		ops->XY0to56(r, 0);		block->mem().mov(m_checks[0], r);
		ops->XY1to56(r, 0);		block->mem().mov(m_checks[1], r);
		ops->XY0to56(r, 1);		block->mem().mov(m_checks[2], r);
		ops->XY1to56(r, 1);		block->mem().mov(m_checks[3], r);
	}

	void JitUnittests::conversion_verify()
	{
		verify(m_checks[0] == 0x0000112233000000);
		verify(m_checks[1] == 0x00ffffeedd000000);
		verify(m_checks[2] == 0x00fffedcba000000);
		verify(m_checks[3] == 0x0000445566000000);
	}

	void JitUnittests::signextend_build()
	{
		m_checks[0] = 0xabcdef;
		m_checks[1] = 0x123456;
		m_checks[2] = 0xabcdef123456;
		m_checks[3] = 0x123456abcdef;
		m_checks[4] = 0xab123456abcdef;
		m_checks[5] = 0x12123456abcdef;

		const DSPReg regLA(*block, PoolReg::DspLA, true, false);
		const DSPReg regLC(*block, PoolReg::DspLC, true, false);
		const DSPReg regSR(*block, PoolReg::DspSR, true, false);
		const DSPReg regA(*block, PoolReg::DspA, true, false);
		const RegGP ra(*block);
		const RegGP rb(*block);

		block->mem().mov(regLA, m_checks[0]);
		block->mem().mov(regLC, m_checks[1]);
		block->mem().mov(regSR, m_checks[2]);
		block->mem().mov(regA, m_checks[3]);
		block->mem().mov(ra, m_checks[4]);
		block->mem().mov(rb, m_checks[5]);

		ops->signextend24to56(r64(regLA.get()));
		ops->signextend24to56(r64(regLC.get()));
		ops->signextend48to56(r64(regSR.get()));
		ops->signextend48to56(r64(regA.get()));
		ops->signextend56to64(ra);
		ops->signextend56to64(rb);

		block->mem().mov(m_checks[0], regLA);
		block->mem().mov(m_checks[1], regLC);
		block->mem().mov(m_checks[2], regSR);
		block->mem().mov(m_checks[3], regA);
		block->mem().mov(m_checks[4], ra);
		block->mem().mov(m_checks[5], rb);
	}

	void JitUnittests::signextend_verify()
	{
		verify(m_checks[0] == 0xffffffffabcdef);
		verify(m_checks[1] == 0x00000000123456);

		verify(m_checks[2] == 0xffabcdef123456);
		verify(m_checks[3] == 0x00123456abcdef);

		verify(m_checks[4] == 0xffab123456abcdef);
		verify(m_checks[5] == 0x0012123456abcdef);
	}

	void JitUnittests::ccr_u_build()
	{
		dsp.regs().sr.var = 0;

		m_checks[0] = 0xee012233445566;
		m_checks[1] = 0xee412233445566;
		m_checks[2] = 0xee812233445566;
		m_checks[3] = 0xeec12233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(*block);

			block->asm_().mov(r, asmjit::Imm(m_checks[i]));
			ops->ccr_u_update(r);
			block->mem().mov(m_checks[i], block->regs().getSR(JitDspRegs::Read));
		}
	}

	void JitUnittests::ccr_u_verify()
	{
		verify((m_checks[0] & CCR_U));
		verify(!(m_checks[1] & CCR_U));
		verify(!(m_checks[2] & CCR_U));
		verify((m_checks[3] & CCR_U));
	}

	void JitUnittests::ccr_e_build()
	{
		dsp.regs().sr.var = 0;

		m_checks[0] = 0xff812233445566;
		m_checks[1] = 0xff712233445566;
		m_checks[2] = 0x00712233445566;
		m_checks[3] = 0x00812233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(*block);

			block->asm_().mov(r, asmjit::Imm(m_checks[i]));
			ops->ccr_e_update(r);
			block->mem().mov(m_checks[i], block->regs().getSR(JitDspRegs::Read));

		}
	}

	void JitUnittests::ccr_e_verify()
	{
		verify(!(m_checks[0] & CCR_E));
		verify((m_checks[1] & CCR_E));
		verify(!(m_checks[2] & CCR_E));
		verify((m_checks[3] & CCR_E));
	}

	void JitUnittests::ccr_n_build()
	{
		m_checks[0] = 0xff812233445566;
		m_checks[1] = 0x7f812233445566;

		for(auto i=0; i<2; ++i)
		{
			RegGP r(*block);

			block->asm_().mov(r, asmjit::Imm(m_checks[i]));
			ops->ccr_n_update_by55(r);
			block->mem().mov(m_checks[i], block->regs().getSR(JitDspRegs::Read));
		}
	}

	void JitUnittests::ccr_n_verify()
	{
		verify((m_checks[0] & CCR_N));
		verify(!(m_checks[1] & CCR_N));
	}

	void JitUnittests::ccr_s_build()
	{
		m_checks[0] = 0xff0fffffffffff;
		m_checks[1] = 0xff2fffffffffff;
		m_checks[2] = 0xff4fffffffffff;
		m_checks[3] = 0xff6fffffffffff;

		dsp.regs().sr.var = 0;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(*block);

			block->asm_().mov(r, asmjit::Imm(m_checks[i]));
			block->asm_().clr(block->regs().getSR(JitDspRegs::Write));
			ops->ccr_s_update(r);
			block->mem().mov(m_checks[i], block->regs().getSR(JitDspRegs::Read));
		}
	}

	void JitUnittests::ccr_s_verify()
	{
		verify(m_checks[0] == 0);
		verify(m_checks[1] == CCR_S);
		verify(m_checks[2] == CCR_S);
		verify(m_checks[3] == 0);
	}

	void JitUnittests::agu_build()
	{
		dsp.regs().r[0].var = 0x1000;
		dsp.regs().n[0].var = 0x10;
		dsp.set_m(0, 0xffffff);

		uint32_t ci=0;

		DspValue temp(*block);

		{
			auto r = ops->updateAddressRegister(MMM_Rn, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}

		{
			auto r = ops->updateAddressRegister(MMM_RnPlus, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}

		{
			auto r = ops->updateAddressRegister(MMM_RnMinus, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}

		{
			auto r = ops->updateAddressRegister(MMM_RnPlusNn, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}

		{
			auto r = ops->updateAddressRegister(MMM_RnMinusNn, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}

		{
			auto r = ops->updateAddressRegister(MMM_RnPlusNnNoUpdate, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}

		{
			auto r = ops->updateAddressRegister(MMM_MinusRn, 0);
			block->mem().mov(m_checks[ci++], r.get());
			r.release();
			r = block->regs().getR(0);
			block->mem().mov(m_checks[ci++], r.get());
		}
	}

	void JitUnittests::agu_verify()
	{
		verify(m_checks[0 ] == 0x1000);	verify(m_checks[1 ] == 0x1000);
		verify(m_checks[2 ] == 0x1000);	verify(m_checks[3 ] == 0x1001);
		verify(m_checks[4 ] == 0x1001);	verify(m_checks[5 ] == 0x1000);
		verify(m_checks[6 ] == 0x1000);	verify(m_checks[7 ] == 0x1010);
		verify(m_checks[8 ] == 0x1010);	verify(m_checks[9 ] == 0x1000);
		verify(m_checks[10] == 0x1010);	verify(m_checks[11] == 0x1000);
		verify(m_checks[12] == 0x0fff);	verify(m_checks[13] == 0x0fff);
	}

	void JitUnittests::agu_modulo_build()
	{
		dsp.regs().r[0].var = 0x100;
		dsp.regs().n[0].var = 0x200;
		dsp.set_m(0, 0xfff);

		DspValue temp(*block);

		for(size_t i=0; i<8; ++i)
		{
			m_checks[i] = 0;
			ops->updateAddressRegister(MMM_RnPlusNn, 0);
			block->regs().getR(temp, 0);
			block->mem().mov(m_checks[i], temp.get());
			temp.release();
		}
	}

	void JitUnittests::agu_modulo_verify()
	{
		verify(m_checks[0] == 0x300);
		verify(m_checks[1] == 0x500);
		verify(m_checks[2] == 0x700);
		verify(m_checks[3] == 0x900);
		verify(m_checks[4] == 0xb00);
		verify(m_checks[5] == 0xd00);
		verify(m_checks[6] == 0xf00);
		verify(m_checks[7] == 0x100);
	}

	void JitUnittests::agu_modulo2_build()
	{
		dsp.regs().r[0].var = 0x70;
		dsp.regs().n[0].var = 0x20;
		dsp.set_m(0, 0x100);

		DspValue temp(*block);

		for(size_t i=0; i<8; ++i)
		{
			ops->updateAddressRegister(MMM_RnMinusNn, 0);
			block->regs().getR(temp, 0);
			block->mem().mov(m_checks[i], temp.get());
			temp.release();
		}
	}

	void JitUnittests::agu_modulo2_verify()
	{
		verify(m_checks[0] == 0x50);
		verify(m_checks[1] == 0x30);
		verify(m_checks[2] == 0x10);
		verify(m_checks[3] == 0xf1);
		verify(m_checks[4] == 0xd1);
		verify(m_checks[5] == 0xb1);
		verify(m_checks[6] == 0x91);
		verify(m_checks[7] == 0x71);
	}

	void JitUnittests::transferSaturation_build()
	{
		const RegGP temp(*block);

		block->asm_().mov(temp, asmjit::Imm(0x00ff700000555555));
		ops->transferSaturation24(temp, temp);
		block->mem().mov(m_checks[0], temp);

		block->asm_().mov(temp, asmjit::Imm(0x00008abbcc555555));
		ops->transferSaturation24(temp, temp);
		block->mem().mov(m_checks[1], temp);

		block->asm_().mov(temp, asmjit::Imm(0x0000334455667788));
		ops->transferSaturation24(temp, temp);
		block->mem().mov(m_checks[2], temp);
	}

	void JitUnittests::transferSaturation_verify()
	{
		verify(m_checks[0] == 0x800000);
		verify(m_checks[1] == 0x7fffff);
		verify(m_checks[2] == 0x334455);
	}

	void JitUnittests::transferSaturation48()
	{
		runTest([&]()
		{
			const RegGP temp(*block);

			block->asm_().mov(temp, asmjit::Imm(0x00ff700000555555));
			ops->transferSaturation48(temp, temp);
			block->mem().mov(m_checks[0], temp);

			block->asm_().mov(temp, asmjit::Imm(0x00008abbcc555555));
			ops->transferSaturation48(temp, temp);
			block->mem().mov(m_checks[1], temp);

			block->asm_().mov(temp, asmjit::Imm(0x0000334455667788));
			ops->transferSaturation48(temp, temp);
			block->mem().mov(m_checks[2], temp);

			block->asm_().mov(temp, asmjit::Imm(0x00fffefefefefefe));
			ops->transferSaturation48(temp, temp);
			block->mem().mov(m_checks[3], temp);
		}, [&]()
		{
			verify(m_checks[0] == 0x800000000000);
			verify(m_checks[1] == 0x7fffffffffff);
			verify(m_checks[2] == 0x334455667788);
			verify(m_checks[3] == 0xfefefefefefe);
		}
		);
	}

	void JitUnittests::testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq)
	{
		dsp.regs().a.var = _value;
		runTest([&]()
		{
			const RegGP r(*block);
			block->asm_().mov(r, asmjit::Imm(_compareValue));
			ops->alu_cmp(0, r, false);

			ops->decode_cccc(r.get(), CCCC_LessThan);			block->mem().mov(m_checks[0], r.get());
			ops->decode_cccc(r.get(), CCCC_LessEqual);			block->mem().mov(m_checks[1], r.get());
			ops->decode_cccc(r.get(), CCCC_Equal);				block->mem().mov(m_checks[2], r.get());
			ops->decode_cccc(r.get(), CCCC_GreaterEqual);		block->mem().mov(m_checks[3], r.get());
			ops->decode_cccc(r.get(), CCCC_GreaterThan);		block->mem().mov(m_checks[4], r.get());
			ops->decode_cccc(r.get(), CCCC_NotEqual);			block->mem().mov(m_checks[5], r.get());
		}, [&]()
		{
			verify(_lt == (dsp.decode_cccc(CCCC_LessThan) != 0));
			verify(_le == (dsp.decode_cccc(CCCC_LessEqual) != 0));
			verify(_eq == (dsp.decode_cccc(CCCC_Equal) != 0));
			verify(_ge == (dsp.decode_cccc(CCCC_GreaterEqual) != 0));
			verify(_gt == (dsp.decode_cccc(CCCC_GreaterThan) != 0));
			verify(_neq == (dsp.decode_cccc(CCCC_NotEqual) != 0));	

			verify(_lt  == (m_checks[0] != 0));
			verify(_le  == (m_checks[1] != 0));
			verify(_eq  == (m_checks[2] != 0));
			verify(_ge  == (m_checks[3] != 0));
			verify(_gt  == (m_checks[4] != 0));
			verify(_neq == (m_checks[5] != 0));	
		}
		);
	}

	void JitUnittests::decode_dddddd_write()
	{
		runTest([&]()
		{
			m_checks.fill(0);

			DspValue r(*block);

			for(int i=0; i<8; ++i)
			{
				const TWord inc = i * 0x10000;

				emit(0x60f400 + inc, 0x110000 * (i+1));		// move #$110000,ri
				block->regs().getR(r, i);
				block->mem().mov(m_checks[i], r32(r.get()));

				emit(0x70f400 + inc, 0x001100 * (i+1));		// move #$001100,ni
				block->regs().getN(r, i);
				block->mem().mov(m_checks[i+8], r32(r.get()));

				emit(0x05f420 + i, 0x000011 * (i+1));		// move #$000011,mi
				block->regs().getM(r, i);
				block->mem().mov(m_checks[i+16], r32(r.get()));
			}
		}, [&]()
		{
			for(size_t i=0; i<8; ++i)
			{
				verify(m_checks[i   ] == 0x110000 * (i+1));
				verify(m_checks[i+8 ] == 0x001100 * (i+1));
				verify(m_checks[i+16] == 0x000011 * (i+1));
			}
		});

		runTest([&]()
		{
			m_checks.fill(0);

			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;

			int i=0;
			const RegGP r(*block);

			emit(0x50f400, 0x111111);	// move #$111111,a0
			emit(0x51f400, 0x222222);	// move #$222222,b0

			block->regs().getALU(r, 0);	block->mem().mov(m_checks[i++], r.get());
			block->regs().getALU(r, 1);	block->mem().mov(m_checks[i++], r.get());

			emit(0x54f400, 0x111111);	// move #$111111,a1
			emit(0x55f400, 0x222222);	// move #$222222,b1

			block->regs().getALU(r, 0);	block->mem().mov(m_checks[i++], r.get());
			block->regs().getALU(r, 1);	block->mem().mov(m_checks[i++], r.get());

			emit(0x56f400, 0x111111);	// move #$111111,a
			emit(0x57f400, 0x222222);	// move #$222222,b

			block->regs().getALU(r, 0);	block->mem().mov(m_checks[i++], r.get());
			block->regs().getALU(r, 1);	block->mem().mov(m_checks[i++], r.get());

			emit(0x52f400, 0x111111);	// move #$111111,a2
			emit(0x53f400, 0x222222);	// move #$222222,b2

			block->regs().getALU(r, 0);	block->mem().mov(m_checks[i++], r.get());
			block->regs().getALU(r, 1);	block->mem().mov(m_checks[i], r.get());
		}, [&]()
		{
			int i=0;
			verify(m_checks[i++] == 0x00000000111111);
			verify(m_checks[i++] == 0x00000000222222);
			verify(m_checks[i++] == 0x00111111111111);
			verify(m_checks[i++] == 0x00222222222222);
			verify(m_checks[i++] == 0x00111111000000);
			verify(m_checks[i++] == 0x00222222000000);
			verify(m_checks[i++] == 0x11111111000000);
			verify(m_checks[i++] == 0x22222222000000);
		});

		runTest([&]()
		{
			m_checks.fill(0);
			dsp.x0(0xaaaaaa);
			dsp.x1(0xbbbbbb);
			dsp.y0(0xcccccc);
			dsp.y1(0xdddddd);

			int i=0;
			const RegGP r(*block);

			emit(0x44f400, 0x111111);	// move #$111111,x0
			block->regs().getXY(r, 0);
			block->mem().mov(m_checks[i++], r.get());

			emit(0x45f400, 0x222222);	// move #$222222,x1
			block->regs().getXY(r, 0);
			block->mem().mov(m_checks[i++], r.get());

			emit(0x46f400, 0x333333);	// move #$333333,y0
			block->regs().getXY(r, 1);
			block->mem().mov(m_checks[i++], r.get());

			emit(0x47f400, 0x444444);	// move #$444444,y1
			block->regs().getXY(r, 1);
			block->mem().mov(m_checks[i], r.get());

		}, [&]()
		{
			int i=0;
			verify(m_checks[i++] == 0xbbbbbb111111);
			verify(m_checks[i++] == 0x222222111111);
			verify(m_checks[i++] == 0xdddddd333333);
			verify(m_checks[i++] == 0x444444333333);
		});
	}

	void JitUnittests::decode_dddddd_read()
	{
		runTest([&]()
		{
			m_checks.fill(0);

			DspValue r(*block);

			for(int i=0; i<8; ++i)
			{
				const TWord inc = i * 0x10000;

				dsp.regs().r[i].var = (i+1) * 0x110000;
				dsp.regs().n[i].var = (i+1) * 0x001100;
				dsp.set_m(i, (i+1) * 0x000011);

				emit(0x600500 + inc);						// asm move ri,x:$5
				block->mem().readDspMemory(r, MemArea_X, 0x5);
				block->mem().mov(m_checks[i], r32(r.get()));

				emit(0x700500 + inc);						// asm move ni,x:$5
				block->mem().readDspMemory(r, MemArea_X, 0x5);
				block->mem().mov(m_checks[i+8], r32(r.get()));

				emit(0x050520 + i);							// asm move mi,x:$5
				block->mem().readDspMemory(r, MemArea_X, 0x5);
				block->mem().mov(m_checks[i+16], r32(r.get()));
			}
		}, [&]()
		{
			for(size_t i=0; i<8; ++i)
			{
				verify(m_checks[i   ] == 0x110000 * (i+1));
				verify(m_checks[i+8 ] == 0x001100 * (i+1));
				verify(m_checks[i+16] == 0x000011 * (i+1));
			}
		});

		runTest([&]()
		{
			m_checks.fill(0);
			dsp.regs().a.var = 0x11112233445566;
			dsp.regs().b.var = 0xff5566778899aa;

			int i=0;
			DspValue r(*block);

			emit(0x560500);								// a,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x570500);								// b,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x500500);								// a0,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x540500);								// a1,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x520500);								// a2,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x510500);								// b0,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x550500);								// b1,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));

			emit(0x530500);								// b2,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i], r32(r.get()));
		}, [&]()
		{
			int i=0;
			verify(m_checks[i++] == 0x7fffff); // a
			verify(m_checks[i++] == 0x800000); // b
			verify(m_checks[i++] == 0x445566); // a0
			verify(m_checks[i++] == 0x112233); // a1
			verify(m_checks[i++] == 0x000011); // a2
			verify(m_checks[i++] == 0x8899aa); // b0
			verify(m_checks[i++] == 0x556677); // b1
			verify(m_checks[i++] == 0xffffff); // b2
		});

		runTest([&]()
		{
			m_checks.fill(0);
			dsp.x0(0xaaabbb);
			dsp.x1(0xcccddd);
			dsp.y0(0xeeefff);
			dsp.y1(0x111222);

			int i=0;
			DspValue r(*block);

			emit(0x440500);								// move x0,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));
			emit(0x450500);								// move x1,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));
			emit(0x460500);								// move y0,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i++], r32(r.get()));
			emit(0x470500);								// move y1,x:$5
			block->mem().readDspMemory(r, MemArea_X, 0x5);
			block->mem().mov(m_checks[i], r32(r.get()));
		}, [&]()
		{
			int i=0;
			verify(m_checks[i++] == 0xaaabbb);
			verify(m_checks[i++] == 0xcccddd);
			verify(m_checks[i++] == 0xeeefff);
			verify(m_checks[i++] == 0x111222);
		});
	}

	void JitUnittests::getSS_build()
	{
		const RegGP temp(*block);

		dsp.regs().sp.var = 0xf0;

		for(int i=0; i<dsp.regs().ss.eSize; ++i)
		{			
			dsp.regs().ss[i].var = 0x111111111111 * i;

			block->regs().getSS(temp);
			ops->incSP();
			block->mem().mov(m_checks[i], temp);
		}
	}

	void JitUnittests::getSS_verify()
	{
		for(int i=0; i<dsp.regs().ss.eSize; ++i)
			verify(dsp.regs().ss[i].var == 0x111111111111 * i);
	}

	void JitUnittests::getSetRegs_build()
	{
		dsp.regs().ep.var = 0x112233;
		dsp.regs().vba.var = 0x223344;
		dsp.regs().sc.var = 0x33;
		dsp.regs().sz.var = 0x556677;
		dsp.regs().omr.var = 0x667788;
		dsp.regs().sp.var = 0x778899;
		dsp.regs().la.var = 0x8899aa;
		dsp.regs().lc.var = 0x99aabb;

		dsp.regs().a.var = 0x00ffaabbcc112233;
		dsp.regs().b.var = 0x00ee112233445566;

		dsp.regs().x.var = 0x0000aabbccddeeff;

		const RegGP temp(*block);
		const auto r = r32(temp.get());

		auto& regs = block->regs();

		auto modify = [&](const JitRegGP& t)
		{
			block->asm_().shl(r32(t), asmjit::Imm(4));

#ifdef HAVE_ARM64
			block->asm_().and_(r32(t), r32(t), asmjit::Imm(0xffffff));
#else
			block->asm_().and_(r32(t), asmjit::Imm(0xffffff));
#endif
		};

		auto modify64 = [&](const JitRegGP& t)
		{
			block->asm_().shl(r64(t), asmjit::Imm(4));
		};

		DspValue v32(*block, UsePooledTemp);

		regs.getEP(v32);				modify(v32.get());		regs.setEP(v32);
		regs.getVBA(v32);				modify(v32.get());		regs.setVBA(v32);
		regs.getSC(v32);				modify(v32.get());		regs.setSC(v32);
		regs.getSZ(v32);				modify(v32.get());		regs.setSZ(v32);
		regs.getOMR(v32);				modify(v32.get());		regs.setOMR(v32);
		regs.getSP(v32);				modify(v32.get());		regs.setSP(v32);
		regs.getLA(v32);				modify(v32.get());		regs.setLA(v32);
		regs.getLC(v32);				modify(v32.get());		regs.setLC(v32);

		ops->getALU0(v32, 0);			modify64(v32.get());		ops->setALU0(0,v32);
		ops->getALU1(v32, 0);			modify64(v32.get());		ops->setALU1(0,v32);
		ops->getALU2signed(v32, 0);	modify64(v32.get());		ops->setALU2(0,v32);

		DspValue v64(*block, UsePooledTemp);
		v64.temp(DspValue::Temp56);

		regs.getALU(v64.get(), 1);			modify64(v64.get());	regs.setALU(1, v64);

		ops->getXY0(v32, 0, false);	modify(v32.get());		ops->setXY0(0, v32);
		ops->getXY1(v32, 0, false);	modify(v32.get());		ops->setXY1(0, v32);
	}

	void JitUnittests::getSetRegs_verify()
	{
		auto& r = dsp.regs();

		verify(r.ep.var == 0x122330);
		verify(r.vba.var == 0x233440);
		verify(r.sc.var == 0x30);
		verify(r.sz.var == 0x566770);
		verify(r.omr.var == 0x677880);
		verify(r.sp.var == 0x788990);
		verify(r.la.var == 0x899aa0);
		verify(r.lc.var == 0x9aabb0);

		verify(r.a.var == 0x00f0abbcc0122330);
		verify(r.b.var == 0x00e1122334455660);

		verify(r.x.var == 0x0000abbcc0deeff0);
	}

	void JitUnittests::jitDiv()
	{
		static constexpr uint64_t expectedValues[24] =
		{
			0xffef590e000000,
			0xffef790e000000,
			0xffefb90e000000,
			0xfff0390e000000,
			0xfff1390e000000,
			0xfff3390e000000,
			0xfff7390e000000,
			0xffff390e000000,
			0x000f390e000000,
			0x000dab2a000001,
			0x000a8f62000003,
			0x000457d2000007,
			0xfff7e8b200000f,
			0x0000985600001e,
			0xfff069ba00003d,
			0xfff19a6600007a,
			0xfff3fbbe0000f4,
			0xfff8be6e0001e8,
			0x000243ce0003d0,
			0xfff3c0aa0007a1,
			0xfff84846000f42,
			0x0001577e001e84,
			0xfff1e80a003d09,
			0xfff49706007a12
		};

		dsp.setSR(dsp.getSR().var & 0xfe);
		dsp.regs().a.var = 0x00001000000000;
		dsp.regs().y.var =   0x04444410c6f2;

		runTest([&]()
		{
			for(size_t i=0; i<24; ++i)
			{
				emit(0x018050);		// div y0,a
				RegGP r(*block);
				block->regs().getALU(r, 0);
				block->mem().mov(m_checks[i], r.get());
			}
		},
		[&]()
		{
			for(size_t i=0; i<24; ++i)
			{
				verify(m_checks[i] == expectedValues[i]);
			}
		});
	}

	void JitUnittests::rep_div()
	{
		{
			// regular mode for comparison

			dsp.y0(0x218dec);
			dsp.regs().a.var = 0x00008000000000;
			dsp.setSR(0x0800d4);

			static constexpr uint64_t expectedValues[24] =
			{
				0xffdf7214000000,
				0xffe07214000000,
				0xffe27214000000,
				0xffe67214000000,
				0xffee7214000000,
				0xfffe7214000000,
				0x001e7214000000,
				0x001b563c000001,
				0x00151e8c000003,
				0x0008af2c000007,
				0xffefd06c00000f,
				0x00012ec400001e,
				0xffe0cf9c00003d,
				0xffe32d2400007a,
				0xffe7e8340000f4,
				0xfff15e540001e8,
				0x00044a940003d0,
				0xffe7073c0007a1,
				0xffef9c64000f42,
				0x0000c6b4001e84,
				0xffdfff7c003d09,
				0xffe18ce4007a12,
				0xffe4a7b400f424,
				0xffeadd5401e848
			};

			for (size_t i = 0; i < 24; ++i)
			{
				runTest([&]()
				{

					emit(0x018050);	// div y0,a
				},
					[&]()
				{
					verify(dsp.regs().a.var == static_cast<int64_t>(expectedValues[i]));
				});
			}
		}

		runTest([&]()
		{
			dsp.y0(0x218dec);
			dsp.regs().a.var = 0x00008000000000;
			dsp.setSR(0x0800d4);

			dsp.memory().set(MemArea_P, 0, 0x0618a0);	// rep #<18
			dsp.memory().set(MemArea_P, 1, 0x018050);	// div y0,a

			ops->emit(0);
		},
		[&]()
		{
			verify(dsp.regs().a.var == 0xffeadd5401e848);
			verify(dsp.getSR().var == 0x0800d4);
		});
	}

	void JitUnittests::parallelMoveXY()
	{
		runTest([&]()
		{
			dsp.set_m(0, 0xff);
			dsp.set_m(4, 0xff);

			emit(0xf09818);	// add a,b		x:(r0)+,x0		y:(r4)+,y0
			emit(0x950818);	// add a,b		x1,x:(r0)+n0	y1,y:(r4)+n4
			emit(0xbb9818);	// add a,b		x:(r0)+,a		b,y:(r4)+
		}, [&]()
		{
			dsp.set_m(0, 0xffffff);
			dsp.set_m(4, 0xffffff);
		});
	}

	void JitUnittests::emit(const TWord _opA, TWord _opB, TWord _pc)
	{
		JitDspMode mode;

		if constexpr(g_useDspMode)
		{
			mode.initialize(dsp);
			block->setMode(&mode);
		}

		block->asm_().nop();
		ops->emit(_pc, _opA, _opB);
		block->asm_().nop();

		if constexpr(g_useDspMode)
			block->setMode(nullptr);
	}
}
