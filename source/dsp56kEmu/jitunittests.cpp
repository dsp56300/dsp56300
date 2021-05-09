#include "jitunittests.h"

#include "jitblock.h"
#include "jitops.h"

namespace dsp56k
{
	JitUnittests::JitUnittests()
	: mem(m_defaultMemoryValidator, 0x100)
	, dsp(mem, &peripherals, &peripherals)
	, m_checks({})
	{
		runTest(&JitUnittests::conversion_build, &JitUnittests::conversion_verify);
		runTest(&JitUnittests::signextend_build, &JitUnittests::signextend_verify);

		runTest(&JitUnittests::ccr_u_build, &JitUnittests::ccr_u_verify);
		runTest(&JitUnittests::ccr_e_build, &JitUnittests::ccr_e_verify);
		runTest(&JitUnittests::ccr_n_build, &JitUnittests::ccr_n_verify);
		runTest(&JitUnittests::ccr_s_build, &JitUnittests::ccr_s_verify);

		runTest(&JitUnittests::abs_build, &JitUnittests::abs_verify);
		
		runTest(&JitUnittests::add_build, &JitUnittests::add_verify);
		runTest(&JitUnittests::addShortImmediate_build, &JitUnittests::addShortImmediate_verify);
		runTest(&JitUnittests::addLongImmediate_build, &JitUnittests::addLongImmediate_verify);
		runTest(&JitUnittests::addl_build, &JitUnittests::addl_verify);
		runTest(&JitUnittests::addr_build, &JitUnittests::addr_verify);

		runTest(&JitUnittests::and_build, &JitUnittests::and_verify);

		runTest(&JitUnittests::andi_build, &JitUnittests::andi_verify);
		runTest(&JitUnittests::ori_build, &JitUnittests::ori_verify);
		
		runTest(&JitUnittests::clr_build, &JitUnittests::clr_verify);
	}

	void JitUnittests::runTest(void( JitUnittests::* _build)(JitBlock&, JitOps&), void( JitUnittests::* _verify)())
	{
		asmjit::CodeHolder code;
		code.init(m_rt.environment());

		asmjit::x86::Assembler m_asm(&code);

		{
			JitBlock block(m_asm, dsp);

			JitOps ops(block, false);

			(this->*_build)(block, ops);
		}

		m_asm.ret();

		typedef void (*Func)();
		Func func;
		const auto err = m_rt.add(&func, &code);
		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		func();

		(this->*_verify)();

		m_rt.release(&func);
	}

	void JitUnittests::conversion_build(JitBlock& _block, JitOps& _ops)
	{
		_block.asm_().bind(_block.asm_().newNamedLabel("test_conv"));

		dsp.regs().x.var = 0xffeedd112233;
		dsp.regs().y.var = 0x112233445566;

		const RegGP r0(_block.gpPool());
		const RegGP r1(_block.gpPool());

		_ops.XY0to56(r0, 0);
		_ops.XY1to56(r1, 0);

		_block.mem().mov(m_checks[0], r0);
		_block.mem().mov(m_checks[1], r1);
	}

	void JitUnittests::conversion_verify()
	{
		assert(m_checks[0] == 0x0000112233000000);
		assert(m_checks[1] == 0x00ffffeedd000000);
	}

	void JitUnittests::signextend_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xabcdef;
		m_checks[1] = 0x123456;
		m_checks[2] = 0xabcdef123456;
		m_checks[3] = 0x123456abcdef;
		m_checks[4] = 0xab123456abcdef;
		m_checks[5] = 0x12123456abcdef;

		const RegGP ra(_block.gpPool());
		const RegGP rb(_block.gpPool());

		_block.mem().mov(regPC, m_checks[0]);
		_block.mem().mov(regLC, m_checks[1]);
		_block.mem().mov(regSR, m_checks[2]);
		_block.mem().mov(regLA, m_checks[3]);
		_block.mem().mov(ra, m_checks[4]);
		_block.mem().mov(rb, m_checks[5]);

		_ops.signextend24to56(regPC);
		_ops.signextend24to56(regLC);
		_ops.signextend48to56(regSR);
		_ops.signextend48to56(regLA);
		_ops.signextend56to64(ra);
		_ops.signextend56to64(rb);

		_block.mem().mov(m_checks[0], regPC);
		_block.mem().mov(m_checks[1], regLC);
		_block.mem().mov(m_checks[2], regSR);
		_block.mem().mov(m_checks[3], regLA);
		_block.mem().mov(m_checks[4], ra);
		_block.mem().mov(m_checks[5], rb);
	}

	void JitUnittests::signextend_verify()
	{
		assert(m_checks[0] == 0xffffffffabcdef);
		assert(m_checks[1] == 0x00000000123456);

		assert(m_checks[2] == 0xffabcdef123456);
		assert(m_checks[3] == 0x00123456abcdef);

		assert(m_checks[4] == 0xffab123456abcdef);
		assert(m_checks[5] == 0x0012123456abcdef);
	}

	void JitUnittests::ccr_u_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.sr.var = 0;

		m_checks[0] = 0xee012233445566;
		m_checks[1] = 0xee412233445566;
		m_checks[2] = 0xee812233445566;
		m_checks[3] = 0xeec12233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_ops.ccr_u_update(r);
			_block.mem().mov(m_checks[i], regSR);
		}
	}

	void JitUnittests::ccr_u_verify()
	{
		assert((m_checks[0] & SR_U));
		assert(!(m_checks[1] & SR_U));
		assert(!(m_checks[2] & SR_U));
		assert((m_checks[3] & SR_U));
	}

	void JitUnittests::ccr_e_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.sr.var = 0;

		m_checks[0] = 0xff812233445566;
		m_checks[1] = 0xff712233445566;
		m_checks[2] = 0x00712233445566;
		m_checks[3] = 0x00812233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_block.asm_().nop();
			_ops.ccr_e_update(r);
			_block.asm_().nop();
			_block.mem().mov(m_checks[i], regSR);

		}
	}

	void JitUnittests::ccr_e_verify()
	{
		assert(!(m_checks[0] & SR_E));
		assert((m_checks[1] & SR_E));
		assert(!(m_checks[2] & SR_E));
		assert((m_checks[3] & SR_E));
	}

	void JitUnittests::ccr_n_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xff812233445566;
		m_checks[1] = 0x7f812233445566;

		for(auto i=0; i<2; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_block.asm_().nop();
			_ops.ccr_n_update_by55(r);
			_block.asm_().nop();
			_block.mem().mov(m_checks[i], regSR);
		}
	}

	void JitUnittests::ccr_n_verify()
	{
		assert((m_checks[0] & SR_N));
		assert(!(m_checks[1] & SR_N));
	}

	void JitUnittests::ccr_s_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xff0fffffffffff;
		m_checks[1] = 0xff2fffffffffff;
		m_checks[2] = 0xff4fffffffffff;
		m_checks[3] = 0xff6fffffffffff;

		dsp.reg.sr.var = 0;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_block.asm_().mov(regSR, asmjit::Imm(0));
			_block.asm_().nop();
			_ops.ccr_s_update(r);
			_block.asm_().nop();
			_block.mem().mov(m_checks[i], regSR);
		}
	}

	void JitUnittests::ccr_s_verify()
	{
		assert(m_checks[0] == 0);
		assert(m_checks[1] == SR_S);
		assert(m_checks[2] == SR_S);
		assert(m_checks[3] == 0);
	}

	void JitUnittests::abs_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.regs().a.var = 0x00ff112233445566;
		dsp.regs().b.var = 0x0000aabbccddeeff;

		_ops.op_Abs(0x000000);
		_ops.op_Abs(0xffffff);
	}

	void JitUnittests::abs_verify()
	{
		assert(dsp.regs().a == 0x00EEDDCCBBAA9A);
		assert(dsp.regs().b == 0x0000aabbccddeeff);
	}

	void JitUnittests::add_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.regs().a.var = 0x0001e000000000;
		dsp.regs().b.var = 0xfffe2000000000;

		// add b,a
		_ops.emit(0, 0x200010);
	}

	void JitUnittests::add_verify()
	{
		assert(dsp.regs().a.var == 0);
		assert(dsp.sr_test(SR_C));
		assert(dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addShortImmediate_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0;

		// add #<32,a
		_ops.emit(0, 0x017280);
	}

	void JitUnittests::addShortImmediate_verify()
	{
		assert(dsp.regs().a.var == 0x00000032000000);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addLongImmediate_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0;
		dsp.reg.pc.var = 0;

		// add #>32,a, two op add with immediate in extension word
		dsp.mem.set(MemArea_P, 1, 0x000032);
		_ops.emit(0, 0x0140c0);
	}

	void JitUnittests::addLongImmediate_verify()
	{
		assert(dsp.regs().a.var == 0x00000032000000);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addl_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x222222;
		dsp.reg.b.var = 0x333333;

		_ops.emit(0, 0x20001a);
	}

	void JitUnittests::addl_verify()
	{
		assert(dsp.reg.b.var == 0x888888);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addr_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x004edffe000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800d0);							// (S L) U

		_ops.emit(0, 0x200002);	// addr b,a
	}

	void JitUnittests::addr_verify()
	{
		assert(dsp.reg.a.var == 0x0ffb16e12000000);
		assert(dsp.getSR().var == 0x0800c8);			// (S L) N
	}

	void JitUnittests::and_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0xffaabbcc112233;
		dsp.reg.x.var = 0x778899;

		dsp.reg.b.var = 0xaaaabbcc334455;
		dsp.reg.y.var = 0x667788000000;

		_ops.emit(0, 0x200046);	// and x0,a
		_ops.emit(0, 0x20007e);	// and y1,b
	}

	void JitUnittests::and_verify()
	{
		assert(dsp.reg.a.var == 0xff778899112233);
		assert(dsp.reg.b.var == 0xaa667788334455);
	}

	void JitUnittests::andi_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.omr.var = 0xff6666;
		dsp.reg.sr.var = 0xff6666;

		_ops.emit(0, 0x0033ba);	// andi #$33,omr
		_ops.emit(0, 0x0033bb);	// andi #$33,eom
		_ops.emit(0, 0x0033b9);	// andi #$33,ccr
		_ops.emit(0, 0x0033b8);	// andi #$33,mr
	}

	void JitUnittests::andi_verify()
	{
		assert(dsp.reg.omr.var == 0xff2222);
		assert(dsp.reg.sr.var == 0xff2222);
	}

	void JitUnittests::ori_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.omr.var = 0xff1111;
		dsp.reg.sr.var = 0xff1111;

		_ops.emit(0, 0x0022fa);	// ori #$33,omr
		_ops.emit(0, 0x0022fb);	// ori #$33,eom
		_ops.emit(0, 0x0022f9);	// ori #$33,ccr
		_ops.emit(0, 0x0022f8);	// ori #$33,mr
	}

	void JitUnittests::ori_verify()
	{
		assert(dsp.reg.omr.var == 0xff3333);
		assert(dsp.reg.sr.var == 0xff3333);
	}

	void JitUnittests::clr_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0xbada55c0deba5e;

		// ensure that ALU is loaded, otherwise it is not written back to DSP registers
		{
			const RegGP dummy(_block);
			_block.regs().getALU(dummy, 0);			
		}
		_ops.emit(0, 0x200013);
	}

	void JitUnittests::clr_verify()
	{
		assert(dsp.reg.a.var == 0);
	}
}
