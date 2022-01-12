#include "jitemitter.h"

#include "jithelper.h"
#include "jitregtypes.h"

namespace dsp56k
{
#ifdef HAVE_ARM64
	void JitEmitter::and_(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		JitBuilder::and_(_dst, _dst, _imm);
	}

	void JitEmitter::and_(const JitRegGP& _dst, const JitRegGP& _src)
	{
		JitBuilder::and_(_dst, _dst, _src);
	}

	void JitEmitter::add(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		JitBuilder::add(_dst, _dst, _imm);
	}

	void JitEmitter::add(const JitRegGP& _dst, const JitRegGP& _src)
	{
		JitBuilder::add(_dst, _dst, _src);
	}

	void JitEmitter::bsr(const JitRegGP& _dst, const JitReg32& _src)
	{
		clz(r32(_dst), r32(_src));
		neg(_dst);
		add(_dst, _dst, asmjit::Imm(31));
	}

	void JitEmitter::dec(const JitRegGP& _gp)
	{
		sub(_gp, _gp, asmjit::Imm(1));
	}

	void JitEmitter::inc(const JitRegGP& _gp)
	{
		add(_gp, _gp, asmjit::Imm(1));
	}

	void JitEmitter::jz(const asmjit::Label& _label)
	{
		cond_zero().b(_label);
	}

	void JitEmitter::jnz(const asmjit::Label& _label)
	{
		cond_not_zero().b(_label);
	}

	void JitEmitter::jle(const asmjit::Label& _label)
	{
		cond_le().b(_label);
	}

	void JitEmitter::movq(const JitRegGP& _dst, const JitReg128& _src)
	{
		fmov(r64(_dst), _src.d());
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitRegGP& _src)
	{
		fmov(_dst.d(), r64(_src));
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitReg128& _src)
	{
		fmov(_dst, _src);
	}

	void JitEmitter::movd(const JitReg128& _dst, const JitRegGP& _src)
	{
		fmov(_dst.s(), r32(_src));
	}

	void JitEmitter::movd(const JitRegGP& _dst, const JitReg128& _src)
	{
		fmov(r32(_dst), _src.s());
	}

	void JitEmitter::mov(const JitMemPtr& _dst, const JitRegGP& _src)
	{
		str(_src, _dst);
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

	void JitEmitter::neg(const JitRegGP& _reg)
	{
		JitBuilder::neg(_reg, _reg);
	}

	void JitEmitter::or_(const JitRegGP& _gp, const asmjit::Imm& _imm)
	{
		orr(_gp, _gp, _imm);
	}

	void JitEmitter::or_(const JitRegGP& _gp, const JitRegGP& _src)
	{
		orr(_gp, _gp, _src);
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

	void JitEmitter::shl(const JitRegGP& _dst, const JitRegGP& _shift)
	{
		lsl(_dst, _dst, _shift);
	}

	void JitEmitter::shr(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		lsr(_dst, _dst, _imm);
	}

	void JitEmitter::shr(const JitRegGP& _dst, const JitRegGP& _shift)
	{
		lsr(_dst, _dst, _shift);
	}

	void JitEmitter::sal(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		lsl(_dst, _dst, _imm);
	}

	void JitEmitter::sar(const JitRegGP& _dst, const asmjit::Imm& _imm)
	{
		asr(_dst, _dst, _imm);
	}

	void JitEmitter::push(const JitRegGP& _reg)
	{
		// For now, we use the "Use 16-byte stack slots" quick-n-dirty approach. We do not need a lot of stack space anyway
		// https://community.arm.com/developer/ip-products/processors/b/processors-ip-blog/posts/using-the-stack-in-aarch64-implementing-push-and-pop
		str(_reg, ptr_pre(asmjit::a64::regs::sp, -16));
	}

	void JitEmitter::pop(const JitRegGP& _reg)
	{
		ldr(_reg, ptr_post(asmjit::a64::regs::sp, 16));
	}

	void JitEmitter::call(const void* _funcAsPtr)
	{
//		bl(_funcAsPtr);	// this does not work as the range is limited to +/- 32MB, we need to create our own "veneer"
		mov(asmjit::a64::regs::x30, asmjit::Imm(_funcAsPtr));
		blr(asmjit::a64::regs::x30);
	}

	void JitEmitter::sub(const JitRegGP& _dst, const JitRegGP& _src)
	{
		sub(_dst, _dst, _src);
	}

	void JitEmitter::bitTest(const JitRegGP& _src, TWord _bitIndex)
	{
		tst(_src, asmjit::Imm(1ull << _bitIndex));
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
