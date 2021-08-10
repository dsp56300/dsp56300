#pragma once

#include "jitops.h"

namespace dsp56k
{
	template <> inline void JitOps::braOrBsr<Bra>(const JitReg32& _offset) { m_asm.add(_offset, asmjit::Imm(m_pcCurrentOp)); jmp(_offset); }
	template <> inline void JitOps::braOrBsr<Bsr>(const JitReg32& _offset) { m_asm.add(_offset, asmjit::Imm(m_pcCurrentOp)); jsr(_offset); }
}
