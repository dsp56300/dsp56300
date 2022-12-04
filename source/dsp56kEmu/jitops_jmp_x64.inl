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
			m_asm.add(_offset.get(), asmjit::Imm(m_pcCurrentOp));
			jmp(_offset);
		}
	}
	template <> inline void JitOps::braOrBsr<Bsr>(DspValue& _offset)
	{
		if(_offset.isImmediate())
		{
			jsr(m_pcCurrentOp + static_cast<int>(_offset.imm24()));
		}
		else
		{
			m_asm.add(_offset.get(), asmjit::Imm(m_pcCurrentOp));
			jsr(_offset);
		}
	}
}
