#pragma once

#include "jitops.h"

#include "asmjit/core/operand.h"
#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	inline void JitOps::signextend56to64(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
	}

	inline void JitOps::signextend48to56(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(16));
		m_asm.sar(_reg, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
	}

	void JitOps::signextend24to56(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(40));
		m_asm.sar(_reg, asmjit::Imm(32));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
	}

	inline void JitOps::mask56(const RegGP& _alu) const
	{
		m_asm.shl(_alu, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_alu, asmjit::Imm(8));
	}
}
