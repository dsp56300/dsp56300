#pragma once

#include "jitops.h"

namespace dsp56k
{
	template <> inline void JitOps::braOrBsr<Bra>(const JitReg32& _offset)
	{
		m_asm.mov(r32(regReturnVal), asmjit::Imm(m_pcCurrentOp));
		m_asm.add(_offset, r32(regReturnVal));
		jmp(_offset);
	}
	template <> inline void JitOps::braOrBsr<Bsr>(const JitReg32& _offset)
	{
		m_asm.mov(r32(regReturnVal), asmjit::Imm(m_pcCurrentOp));
		m_asm.add(_offset, r32(regReturnVal));
		jsr(_offset);
	}
}
