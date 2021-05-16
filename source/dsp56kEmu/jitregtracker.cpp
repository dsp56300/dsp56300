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

	void AluReg::release()
	{
		m_reg.release();
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

	PushShadowSpace::PushShadowSpace(JitBlock& _block) : m_block(_block)
	{
#ifdef _MSC_VER
		m_block.asm_().push(asmjit::Imm(0xbada55c0deba5e));
		m_block.asm_().push(asmjit::Imm(0xbada55c0deba5e));
		m_block.asm_().push(asmjit::Imm(0xbada55c0deba5e));
		m_block.asm_().push(asmjit::Imm(0xbada55c0deba5e));
#endif
		}

	PushShadowSpace::~PushShadowSpace()
	{
#ifdef _MSC_VER
		const RegGP temp(m_block);
		m_block.asm_().pop(temp);
		m_block.asm_().pop(temp);
		m_block.asm_().pop(temp);
		m_block.asm_().pop(temp);
#endif
	}

	TempGP::TempGP(JitBlock& _block, const JitReg64& _spilledReg): m_block(_block), m_spilledReg(_spilledReg)
	{
		if(!_block.gpPool().empty())
		{
			m_reg = _block.gpPool().get();
			m_mode = ModeTemp;
		}
		else if(!_block.xmmPool().empty())
		{
			m_regXMM = _block.xmmPool().get();
			_block.asm_().movd(m_regXMM, _spilledReg);
			m_reg = _spilledReg;
			m_mode = ModePushToXMM;
		}
		else
		{
			_block.asm_().push(_spilledReg);
			m_reg = _spilledReg;
			m_mode = ModePushToStack;
		}
	}

	TempGP::~TempGP()
	{
		if(m_mode == ModeTemp)
		{
			m_block.gpPool().put(m_reg);
		}
		else if(m_mode == ModePushToXMM)
		{
			m_block.asm_().movd(m_spilledReg, m_regXMM);
			m_block.xmmPool().put(m_regXMM);
		}
		else
		{
			m_block.asm_().pop(m_spilledReg);
		}
	}
}
