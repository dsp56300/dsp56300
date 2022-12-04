#pragma once

#include "jitops.h"

namespace dsp56k
{
	template <> inline void JitOps::braOrBsr<Bra>(DspValue& _offset)
	{
		if (_offset.isImmediate())
		{
			jmp(m_pcCurrentOp + static_cast<int>(_offset.imm24()));
		}
		else
		{
			const RegScratch temp(m_block);
			m_asm.mov(r32(temp), asmjit::Imm(m_pcCurrentOp));
			m_asm.add(_offset.get(), r32(temp));
			jmp(_offset);
		}
	}
	template <> inline void JitOps::braOrBsr<Bsr>(DspValue& _offset)
	{
		if (_offset.isImmediate())
		{
			jsr(m_pcCurrentOp + static_cast<int>(_offset.imm24()));
		}
		else
		{
			const RegScratch temp(m_block);
			m_asm.mov(r32(temp), asmjit::Imm(m_pcCurrentOp));
			m_asm.add(_offset.get(), r32(temp));
			jsr(_offset);
		}
	}
}
