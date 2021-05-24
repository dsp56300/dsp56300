#pragma once

#include "dsp.h"

#include "asmjit/core/jitruntime.h"

namespace dsp56k
{
	class JitBlock;
	class JitOps;

	class JitUnittests
	{
	public:
		JitUnittests();

	private:
		void runTest(void(JitUnittests::*_build)(JitBlock&, JitOps&), void( JitUnittests::*_verify)());
		void runTest(std::function<void(JitBlock&, JitOps&)> _build, std::function<void()> _verify);
		static void nop(JitBlock& _block, size_t _count = 1);
		
		// helper function tests
		void conversion_build(JitBlock& _block, JitOps& _ops);
		void conversion_verify();

		void signextend_build(JitBlock& _block, JitOps& _ops);
		void signextend_verify();

		void ccr_u_build(JitBlock& _block, JitOps& _ops);
		void ccr_u_verify();

		void ccr_e_build(JitBlock& _block, JitOps& _ops);
		void ccr_e_verify();

		void ccr_n_build(JitBlock& _block, JitOps& _ops);
		void ccr_n_verify();

		void ccr_s_build(JitBlock& _block, JitOps& _ops);
		void ccr_s_verify();

		void agu_build(JitBlock& _block, JitOps& _ops);
		void agu_verify();
		
		void agu_modulo_build(JitBlock& _block, JitOps& _ops);
		void agu_modulo_verify();
		
		void agu_modulo2_build(JitBlock& _block, JitOps& _ops);
		void agu_modulo2_verify();
		
		void transferSaturation_build(JitBlock& _block, JitOps& _ops);
		void transferSaturation_verify();

		void testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq);

		// register access tests
		void getSS_build(JitBlock& _block, JitOps& _ops);
		void getSS_verify();

		void getSetRegs_build(JitBlock& _block, JitOps& _ops);
		void getSetRegs_verify();

		// opcode tests
		void abs_build(JitBlock& _block, JitOps& _ops);
		void abs_verify();

		void add_build(JitBlock& _block, JitOps& _ops);
		void add_verify();

		void addShortImmediate_build(JitBlock& _block, JitOps& _ops);
		void addShortImmediate_verify();

		void addLongImmediate_build(JitBlock& _block, JitOps& _ops);
		void addLongImmediate_verify();

		void addl_build(JitBlock& _block, JitOps& _ops);
		void addl_verify();

		void addr_build(JitBlock& _block, JitOps& _ops);
		void addr_verify();

		void and_build(JitBlock& _block, JitOps& _ops);
		void and_verify();

		void andi_build(JitBlock& _block, JitOps& _ops);
		void andi_verify();

		void asl_D_build(JitBlock& _block, JitOps& _ops);
		void asl_D_verify();

		void asl_ii_build(JitBlock& _block, JitOps& _ops);
		void asl_ii_verify();

		void asl_S1S2D_build(JitBlock& _block, JitOps& _ops);
		void asl_S1S2D_verify();

		void asr_D_build(JitBlock& _block, JitOps& _ops);
		void asr_D_verify();

		void asr_ii_build(JitBlock& _block, JitOps& _ops);
		void asr_ii_verify();

		void asr_S1S2D_build(JitBlock& _block, JitOps& _ops);
		void asr_S1S2D_verify();

		void bchg_aa_build(JitBlock& _block, JitOps& _ops);
		void bchg_aa_verify();

		void bclr_ea_build(JitBlock& _block, JitOps& _ops);
		void bclr_ea_verify();

		void bclr_aa_build(JitBlock& _block, JitOps& _ops);
		void bclr_aa_verify();

		void bclr_qqpp_build(JitBlock& _block, JitOps& _ops);
		void bclr_qqpp_verify();

		void bclr_D_build(JitBlock& _block, JitOps& _ops);
		void bclr_D_verify();

		void bset_aa_build(JitBlock& _block, JitOps& _ops);
		void bset_aa_verify();

		void btst_aa_build(JitBlock& _block, JitOps& _ops);
		void btst_aa_verify();

		void cmp();
		void dec();
		void div();
		void dmac();
		void extractu();
		void ifcc();
		void inc();
		void lra();
		void lsl();
		void lsr();
		void lua_ea();
		void lua_rn();
		void mac_S();
		void mpy();
		void mpy_SD();
		void neg();
		void not_();
		void or_();
		void rnd();
		void rol();
		void sub();

		void ori_build(JitBlock& _block, JitOps& _ops);
		void ori_verify();

		void clr_build(JitBlock& _block, JitOps& _ops);
		void clr_verify();

		DefaultMemoryValidator m_defaultMemoryValidator;
		Peripherals56303 peripherals;
		Memory mem;
		DSP dsp;

		asmjit::JitRuntime m_rt;

		std::array<uint64_t,32> m_checks;
	};
}
