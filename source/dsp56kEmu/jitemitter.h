#pragma once

#include "jitregtracker.h"
#include "jittypes.h"

#ifdef HAVE_ARM64
#include "asmjit/arm/a64builder.h"
#include "asmjit/arm/a64assembler.h"
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
		void and_(const JitRegGP& _dst, const JitRegGP& _src);

		void add(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void add(const JitRegGP& _dst, const JitRegGP& _src);

		void bsr(const JitRegGP& _dst, const JitReg32& _src);

		void dec(const JitRegGP& _gp);
		void inc(const JitRegGP& _gp);

		void jz(const asmjit::Label& _label);
		void jnz(const asmjit::Label& _label);
		void jle(const asmjit::Label& _label);

		void movq(const JitRegGP& _dst, const JitReg128& _src);
		void movq(const JitReg128& _dst, const JitRegGP& _src);
		void movq(const JitReg128& _dst, const JitReg128& _src);
		void movdqa(const JitReg128& _dst, const JitReg128& _src);
		void movd(const JitReg128& _dst, const JitRegGP& _src);
		void movd(const JitRegGP& _dst, const JitReg128& _src);

		void mov(const JitMemPtr& _dst, const JitRegGP& _src);
		void mov(const JitRegGP& _dst, const JitMemPtr& _src);
		
		void movd(const JitReg128& _dst, const JitMemPtr& _src);
		void movd(const JitMemPtr& _dst, const JitReg128& _src);

		void movq(const JitReg128& _dst, const JitMemPtr& _src);
		void movq(const JitMemPtr& _dst, const JitReg128& _src);

		void neg(const JitRegGP& _reg);

		void or_(const JitRegGP& _gp, const asmjit::Imm& _imm);
		void or_(const JitRegGP& _gp, const JitRegGP& _src);
		void xor_(const JitRegGP& _a, const JitRegGP& _b);

		void ret();
		void jmp(const asmjit::Label& _label);
		void jge(const asmjit::Label& _label);
		void jg(const asmjit::Label& _label);

		void shl(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void shl(const JitRegGP& _dst, const JitRegGP& _shift);
		void shr(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void shr(const JitRegGP& _dst, const JitRegGP& _shift);
		void sal(const JitRegGP& _dst, const asmjit::Imm& _imm);
		void sar(const JitRegGP& _dst, const asmjit::Imm& _imm);

		void sub(const JitRegGP& _dst, const asmjit::Imm& _imm);

		void push(const JitRegGP& _reg);
		void pop(const JitRegGP& _reg);

		void call(const void* _funcAsPtr);

		void sub(const JitRegGP& _dst, const JitRegGP& _src);
		void cmov(asmjit::arm::CondCode _cc, const JitRegGP& _dst, const JitRegGP& _src);
#else
		static bool hasFeature(asmjit::CpuFeatures::X86::Id _id);
		static bool hasBMI();
		static bool hasBMI2();
		void copyBitToReg(const JitRegGP& _dst, uint32_t _dstBit, const JitRegGP& _src, uint32_t _srcBit);
		void copyBitToReg(const JitRegGP& _dst, const JitRegGP& _src, uint32_t _srcBit)
		{
			copyBitToReg(_dst, 0, _src, _srcBit);
		}

		using JitBuilder::ror;
		using JitBuilder::rol;

		void ror(const JitRegGP& _dst, const JitRegGP& _src, uint32_t _bits);
		void rol(const JitRegGP& _dst, const JitRegGP& _src, uint32_t _bits);
#endif

		void bitTest(const JitRegGP& _src, TWord _bitIndex);

		void move(const JitRegGP& _dst, const JitMemPtr& _src);

		void clr(const JitRegGP& _gp);

		void test_(const JitRegGP& _gp);
		void test_(const JitRegGP& _gp, const asmjit::Imm& _imm);

		void lea_(const JitReg64& _dst, const JitReg64& _src, const void* _ptr, const void* _base);
		bool lea_(const JitReg64& _dst, const JitReg64& _src, int _offset);
	};
}
