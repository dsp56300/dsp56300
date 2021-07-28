#include "jitemitter.h"

#include "jithelper.h"
#include "jitregtypes.h"

namespace dsp56k
{
#ifdef HAVE_ARM64
	void JitEmitter::movq(const JitRegGP& _dst, const JitReg128& _src)
	{
		fmov(r64(_dst), _src);
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitRegGP& _src)
	{
		fmov(_dst, r64(_src));
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitReg128& _src)
	{
		fmov(_dst, _src);
	}

	void JitEmitter::mov(const JitMemPtr& _dst, const JitRegGP& _src)
	{
		str(_src, _dst);
	}

	void JitEmitter::mov(const JitMemPtr& _dst, const asmjit::Imm& _src)
	{
		mov(regReturnVal, _src);
		mov(_dst, regReturnVal);
	}

	void JitEmitter::movd(const JitReg128& _dst, const JitMemPtr& _src)
	{
		ldr(_dst.s(), _src);
	}

	void JitEmitter::movd(const JitMemPtr& _dst, const JitReg128& _src)
	{
		str(_src.s(), _dst);
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitMemPtr& _src)
	{
		ldr(_dst.d(), _src);
	}

	void JitEmitter::movq(const JitMemPtr& _dst, const JitReg128& _src)
	{
		str(_src.d(), _dst);
	}

	void JitEmitter::ret()
	{
		JitBuilder::ret(asmjit::a64::regs::x30);
	}

	void JitEmitter::jmp(const asmjit::Label& _label)
	{
		b(_label);
	}

	void JitEmitter::jge(const asmjit::Label& _label)
	{
		cond_ge().b(_label);
	}

	void JitEmitter::shl(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		lsl(_dst, _dst, _imm);
	}

	void JitEmitter::shr(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		lsr(_dst, _dst, _imm);
	}
#endif

	void JitEmitter::move(const JitRegGP& _dst, const JitMemPtr& _src)
	{
#ifdef HAVE_ARM64
		ldr(_dst, _src);
#else
		mov(_dst, _src);
#endif
	}

	void JitEmitter::clr(const JitRegGP& _gp)
	{
#ifdef HAVE_ARM64
		if(_gp.isGpW())
			JitBuilder::mov(_gp, asmjit::a64::regs::wzr);
		else
			JitBuilder::mov(_gp, asmjit::a64::regs::xzr);
#else
		JitBuilder::xor_(_gp, _gp);
#endif
	}
}
