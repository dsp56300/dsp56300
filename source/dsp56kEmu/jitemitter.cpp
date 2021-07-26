#include "jitemitter.h"

namespace dsp56k
{
#ifdef HAVE_ARM64
	void JitEmitter::movq(const JitRegGP& _dst, const JitReg128& _src)
	{
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitRegGP& _src)
	{
	}

	void JitEmitter::movq(const JitReg128& _dst, const JitReg128& _src)
	{
	}

	void JitEmitter::mov(const JitMemPtr& _dst, const JitRegGP& _src)
	{
	}

	void JitEmitter::mov(const JitMemPtr& _dst, const asmjit::Imm& _src)
	{
	}

	void JitEmitter::movd(const JitMemPtr& _dst, const JitReg128& _src)
	{
	}

	void JitEmitter::movq(const JitMemPtr& _dst, const JitReg128& _src)
	{
	}
#endif
}
