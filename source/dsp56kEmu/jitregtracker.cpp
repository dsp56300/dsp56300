#include "jitregtracker.h"
#include "jitblock.h"

namespace dsp56k
{
	AluReg::AluReg(JitBlock& _block, const TWord _aluIndexSrc, bool readOnly/* = false*/, TWord _aluIndexDst/* = ~0*/)
	: m_block(_block)
	, m_reg(_block)
	, m_aluIndexDst(_aluIndexDst > 2 ? _aluIndexSrc : _aluIndexDst)
	, m_readOnly(readOnly)
	{
		m_block.regs().getALU(m_reg, _aluIndexSrc);
	}

	AluReg::~AluReg()
	{
		if(!m_readOnly)
			m_block.regs().setALU(m_aluIndexDst, m_reg);
	}

	PushGP::PushGP(asmjit::x86::Assembler& _a, const JitReg64& _reg) : m_asm(_a), m_reg(_reg)
	{
		_a.push(m_reg);
	}

	PushGP::~PushGP()
	{
		m_asm.pop(m_reg);
	}

	PushExchange::PushExchange(asmjit::x86::Assembler& _a, const JitReg64& _regA, const JitReg64& _regB) : m_asm(_a), m_regA(_regA), m_regB(_regB)
	{
		swap();
	}

	PushExchange::~PushExchange()
	{
		swap();
	}

	void PushExchange::swap() const
	{
		if(m_regA != m_regB)
			m_asm.xchg(m_regA, m_regB);
	}
}
