#include "jitregtracker.h"
#include "jitblock.h"

namespace dsp56k
{
	AluReg::AluReg(JitBlock& _block, const TWord _aluIndex, bool readOnly/* = false*/) : m_block(_block), m_reg(_block), m_aluIndex(_aluIndex), m_readOnly(readOnly)
	{
		m_block.regs().getALU(m_reg, _aluIndex);
	}

	AluReg::~AluReg()
	{
		if(!m_readOnly)
			m_block.regs().setALU(m_aluIndex, m_reg);
	}

	PushGP::PushGP(JitBlock& _block, const asmjit::x86::Gpq& _reg) : m_block(_block), m_reg(_reg)
	{
		_block.asm_().push(m_reg);
	}

	PushGP::~PushGP()
	{
		m_block.asm_().pop(m_reg);
	}
}
