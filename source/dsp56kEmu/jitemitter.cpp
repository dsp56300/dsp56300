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
		b(asmjit::arm::CondCode::kZero, _label);
	}

	void JitEmitter::jnz(const asmjit::Label& _label)
	{
		b(asmjit::arm::CondCode::kNotZero, _label);
	}

	void JitEmitter::jle(const asmjit::Label& _label)
	{
		b(asmjit::arm::CondCode::kLE, _label);
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

	void JitEmitter::movdqa(const JitReg128& _dst, const JitReg128& _src)
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

	void JitEmitter::mov(const JitRegGP& _dst, const JitMemPtr& _src)
	{
		ldr(_dst, _src);
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

	void JitEmitter::xor_(const JitRegGP& _a, const JitRegGP& _b)
	{
		eor(_a, _a, _b);
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
		b(asmjit::arm::CondCode::kGE, _label);
	}

	void JitEmitter::jg(const asmjit::Label& _label)
	{
		b(asmjit::arm::CondCode::kGT, _label);
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
	void JitEmitter::cmov(asmjit::arm::CondCode _cc, const JitRegGP& _dst, const JitRegGP& _src)
	{
		csel(_dst, _src, _dst, _cc);
	}
#else
	bool JitEmitter::hasFeature(asmjit::CpuFeatures::X86::Id _id) const
	{
		return asmjit::CpuInfo::host().hasFeature(_id);
	}

	bool JitEmitter::hasBMI() const
	{
		return hasFeature(asmjit::CpuFeatures::X86::kBMI);
	}

	bool JitEmitter::hasBMI2() const
	{
		return hasFeature(asmjit::CpuFeatures::X86::kBMI2);
	}

	void JitEmitter::copyBitToReg(const JitRegGP& _dst, const uint32_t _dstBit, const JitRegGP& _src, const uint32_t _srcBit)
	{
		if(_srcBit == _dstBit && _dstBit < 32)
		{
			mov(r32(_dst), r32(_src));														// 0.25
			and_(r32(_dst), asmjit::Imm(1<<_dstBit));										// 0.25
			return;
		}

		if(_dstBit == 0 && _srcBit < 32)
		{
			bitTest(_src, _srcBit);															// 0.25
			setnz(_dst.r8());																// 0.5
			return;
		}

		if (hasBMI2() && _srcBit != _dstBit && _dstBit < 32)
		{
			if(_srcBit < 32)
			{
				if(_dstBit < _srcBit)
					rorx(r32(_dst), r32(_src), asmjit::Imm(_srcBit - _dstBit));				// 0.5
				else
					rorx(r32(_dst), r32(_src), asmjit::Imm(32 - (_dstBit - _srcBit)));		// 0.5
			}
			else
			{
				if(_dstBit < _srcBit)
					rorx(r64(_dst), r64(_src), asmjit::Imm(_srcBit - _dstBit));				// 0.5
				else
					rorx(r64(_dst), r64(_src), asmjit::Imm(64 - (_dstBit - _srcBit)));		// 0.5
			}

			and_(r32(_dst), asmjit::Imm(1<<_dstBit));										// 0.25
			return;
		}

		if (_dstBit >= 32 && _srcBit > 0 && hasBMI2())
		{
			rorx(_dst, r64(_src), asmjit::Imm(_srcBit));									// 0.5
			and_(r32(_dst), asmjit::Imm(1));												// 0.25
			shl(r64(_dst), asmjit::Imm(_dstBit));											// 0.5
			return;
		}

		if(_dstBit >= 32)
			xor_(r64(_dst), r64(_dst));														// 0.25
		else if(_dstBit >= 8)
			xor_(r32(_dst), r32(_dst));														// 0.25

		if(_srcBit >= 32)
		{
			bt(r64(_src), asmjit::Imm(_srcBit));											// 0.5
			setc(_dst.r8());																// 0.5
		}
		else
		{
			bitTest(_src, _srcBit);															// 0.25
			setnz(_dst.r8());																// 0.5
		}

		if(_dstBit >= 32)
			shl(r64(_dst), asmjit::Imm(_dstBit));											// 0.5
		else if(_dstBit > 0)
			shl(r32(_dst), asmjit::Imm(_dstBit));											// 0.5
	}

	void JitEmitter::ror(const JitRegGP& _dst, const JitRegGP& _src, const uint32_t _bits)
	{
		if(hasBMI2())
		{
			if(_dst.size() == 8)
				rorx(_dst, r64(_src), asmjit::Imm(_bits));
			else
				rorx(_dst, r32(_src), asmjit::Imm(_bits));
		}
		else
		{
			if(_src.size() == 4)
				mov(r32(_dst), r32(_src));
			else
				mov(r64(_dst), r64(_src));
			JitBuilder::ror(_dst, asmjit::Imm(_bits));
		}
	}

	void JitEmitter::rol(const JitRegGP& _dst, const JitRegGP& _src, const uint32_t _bits)
	{
		if(_dst.size() == 8)
			ror(_dst, _src, 64 - _bits);
		else
			ror(_dst, _src, 32 - _bits);
	}
#endif

	void JitEmitter::bitTest(const JitRegGP& _src, TWord _bitIndex)
	{
#ifdef HAVE_X86_64
		assert(_bitIndex < 32 && "test only works with 32 bit immediates");
#endif
		if(_bitIndex < 32)
			test_(r32(_src), asmjit::Imm(1u << _bitIndex));
		else
			test_(r64(_src), asmjit::Imm(1ull << _bitIndex));
	}

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
		JitBuilder::xor_(r32(_gp), r32(_gp));
#endif
	}

	void JitEmitter::test_(const JitRegGP& _gp)
	{
#ifdef HAVE_ARM64
		JitBuilder::tst(_gp, _gp);
#else
		JitBuilder::test(_gp, _gp);
#endif
	}

	void JitEmitter::test_(const JitRegGP& _gp, const asmjit::Imm& _imm)
	{
#ifdef HAVE_ARM64
		JitBuilder::tst(_gp, _imm);
#else
		JitBuilder::test(_gp, _imm);
#endif
	}

	void JitEmitter::lea_(const JitReg64& _dst, const JitReg64& _src, void* _ptr, void* _base)
	{
		const auto off = reinterpret_cast<uint64_t>(_ptr) - reinterpret_cast<uint64_t>(_base);
		lea_(_dst, _src, static_cast<int>(off));
	}

	void JitEmitter::lea_(const JitReg64& _dst, const JitReg64& _src, const int _offset)
	{
#ifdef HAVE_ARM64
		// aarch64 add accepts 12 bits as unsigned offset, optionally shifted up by 12 bits
		auto isValidOffset = [](const int32_t off)
		{
			if(off >= 0 && off <= 0xfff)
				return true;
			if(off >= 0 && off <= 0xffffff && (off & 0xfff) == 0)
				return true;
			return false;
		};

		if(isValidOffset(_offset))
		{
			add(_dst, _src, _offset);
		}
		else if(isValidOffset(-_offset))
		{
			sub(_dst, _src, -_offset);
		}
		else
		{
			mov(r32(_dst), asmjit::Imm(_offset));
			add(_dst, _dst, _src);
		}
#else
		lea(_dst, asmjit::x86::ptr(_src, _offset));
#endif
	}
}
