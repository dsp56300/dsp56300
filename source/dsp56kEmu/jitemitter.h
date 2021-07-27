#pragma once

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

		using JitBuilder::mov;

#ifdef HAVE_ARM64
		void movq(const JitRegGP& _dst, const JitReg128& _src);
		void movq(const JitReg128& _dst, const JitRegGP& _src);
		void movq(const JitReg128& _dst, const JitReg128& _src);

		void mov(const JitMemPtr& _dst, const JitRegGP& _src);

		void mov(const JitMemPtr& _dst, const asmjit::Imm& _src);
		
		void movd(const JitReg128& _dst, const JitMemPtr& _src);
		void movd(const JitMemPtr& _dst, const JitReg128& _src);

		void movq(const JitReg128& _dst, const JitMemPtr& _src);
		void movq(const JitMemPtr& _dst, const JitReg128& _src);

		void ret();
		void jmp(const asmjit::Label& _label);
		void jge(const asmjit::Label& _label);

		void shr(const JitRegGP& _dst, const asmjit::Imm& _imm);
#endif

		void move(const JitRegGP& _dst, const JitMemPtr& _src);

		void clr(const JitRegGP& _gp);
	};
}
