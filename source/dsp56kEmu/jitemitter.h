#pragma once

#include "jitregtracker.h"
#include "jittypes.h"

namespace asmjit
{
	class CodeHolder;
}

#ifdef HAVE_ARM64
#include "asmjit/arm/a64builder.h"
#else
#include "asmjit/x86/x86builder.h"
#endif

namespace dsp56k
{
	class JitEmitter : public JitBuilder
	{
	public:
		JitEmitter(asmjit::CodeHolder* _codeHolder) : JitBuilder(_codeHolder) {}

		using JitBuilder::add;
		using JitBuilder::and_;
		using JitBuilder::mov;
		using JitBuilder::sub;

#ifdef HAVE_ARM64
		void and_(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void add(const JitRegGP& _dst, const asmjit::Imm& _imm);

		void bsr(const JitRegGP& _dst, const JitReg32& _src);

		void dec(const JitRegGP& _gp);
		void inc(const JitRegGP& _gp);

		void jz(const asmjit::Label& _label);
		void jnz(const asmjit::Label& _label);

		void movq(const JitRegGP& _dst, const JitReg128& _src);
		void movq(const JitReg128& _dst, const JitRegGP& _src);
		void movq(const JitReg128& _dst, const JitReg128& _src);

		void mov(const JitMemPtr& _dst, const JitRegGP& _src);

		void mov(const JitMemPtr& _dst, const asmjit::Imm& _src);
		
		void movd(const JitReg128& _dst, const JitMemPtr& _src);
		void movd(const JitMemPtr& _dst, const JitReg128& _src);

		void movq(const JitReg128& _dst, const JitMemPtr& _src);
		void movq(const JitMemPtr& _dst, const JitReg128& _src);

		void neg(const JitRegGP& _reg);

		void or_(const JitRegGP& _gp, const asmjit::Imm& _imm);

		void ret();
		void jmp(const asmjit::Label& _label);
		void jge(const asmjit::Label& _label);

		void shl(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void shr(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void sal(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void sar(const JitRegGP& _dst, const asmjit::Imm& _imm);

		void push(const JitRegGP& _reg);
		void pop(const JitRegGP& _reg);

		void call(const void* _funcAsPtr);

		void add(const JitRegGP& _dst, const JitRegGP& _src);
		void sub(const JitRegGP& _dst, const JitRegGP& _src);

		void bitTest(const JitRegGP& _src, TWord _bitIndex);

#endif

		void move(const JitRegGP& _dst, const JitMemPtr& _src);

		void clr(const JitRegGP& _gp);
	};
}
