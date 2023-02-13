#pragma once

#include "asmjit/core/jitruntime.h"

#include <functional>

#include "unittests.h"

namespace dsp56k
{
	class JitBlock;
	class JitOps;

	class JitUnittests : public UnitTests
	{
	public:
		JitUnittests(bool _logging = true);
		virtual ~JitUnittests();

	private:
		void runTest(void(JitUnittests::*_build)(), void(JitUnittests::*_verify)());
		void runTest(const std::function<void()>& _build, const std::function<void()>& _verify) override;
		void nop(size_t _count = 1) const;
		
		// helper function tests
		void conversion_build();
		void conversion_verify();

		void signextend_build();
		void signextend_verify();

		void ccr_u_build();
		void ccr_u_verify();

		void ccr_e_build();
		void ccr_e_verify();

		void ccr_n_build();
		void ccr_n_verify();

		void ccr_s_build();
		void ccr_s_verify();

		void agu_build();
		void agu_verify();
		
		void agu_modulo_build();
		void agu_modulo_verify();
		
		void agu_modulo2_build();
		void agu_modulo2_verify();
		
		void transferSaturation_build();
		void transferSaturation_verify();

		void transferSaturation48();

		void testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq);

		void decode_dddddd_write();
		void decode_dddddd_read();
		
		// register access tests
		void getSS_build();
		void getSS_verify();

		void getSetRegs_build();
		void getSetRegs_verify();

		// opcode tests

		void div();
		void rep_div();

		void x0x1Combinations();

		void emit(TWord _opA, TWord _opB = 0, TWord _pc = 0) override;

		asmjit::JitRuntime m_rt;

		std::array<uint64_t,32> m_checks;

		JitBlock* block = nullptr;
		JitOps* ops = nullptr;
		bool m_logging;
	};
}
