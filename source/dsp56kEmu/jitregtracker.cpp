#include "jitregtracker.h"
#include "jitblock.h"

namespace dsp56k
{
	DSPRegTemp::DSPRegTemp(JitBlock& _block): m_block(_block)
	{
		acquire();
	}

	DSPRegTemp::~DSPRegTemp()
	{
		if (acquired())
			release();
	}

	void DSPRegTemp::acquire()
	{
		m_dspReg = m_block.dspRegPool().aquireTemp();
		m_reg = m_block.dspRegPool().get(m_dspReg, false, false);
		m_block.dspRegPool().lock(m_dspReg);
	}

	void DSPRegTemp::release()
	{
		m_block.dspRegPool().unlock(m_dspReg);
		m_block.dspRegPool().releaseTemp(m_dspReg);
		m_dspReg = JitDspRegPool::DspCount;
	}

	AluReg::AluReg(JitBlock& _block, const TWord _aluIndexSrc, bool readOnly/* = false*/, bool writeOnly/* = false*/, TWord _aluIndexDst/* = ~0*/)
	: m_block(_block)
	, m_reg(_block)
	, m_aluIndexDst(_aluIndexDst > 2 ? _aluIndexSrc : _aluIndexDst)
	, m_readOnly(readOnly)
	{
		if(!writeOnly)
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

	AguReg::AguReg(JitBlock& _block, JitDspRegPool::DspReg _regBase, int _aguIndex, bool readOnly) : DSPReg(_block, static_cast<JitDspRegPool::DspReg>(_regBase + _aguIndex), true, !readOnly)
	{
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

	constexpr bool g_push128Bits = false;
	
	PushXMM::PushXMM(JitBlock& _block, uint32_t _xmmIndex) : m_block(_block), m_xmmIndex(_xmmIndex), m_isLoaded(m_block.dspRegPool().isInUse(asmjit::x86::xmm(_xmmIndex)))
	{
		if(!m_isLoaded)
			return;

		const RegGP r(_block);

		const auto xm = asmjit::x86::xmm(_xmmIndex);
		
		_block.asm_().movq(r, xm);
		_block.asm_().push(r.get());

		if(g_push128Bits)
		{
			_block.asm_().psrldq(xm, asmjit::Imm(8));

			_block.asm_().movq(r, xm);
			_block.asm_().push(r.get());			
		}
	}

	PushXMM::~PushXMM()
	{
		if(!m_isLoaded)
			return;

		const RegGP r(m_block);

		const auto xm = asmjit::x86::xmm(m_xmmIndex);

		m_block.asm_().pop(r.get());
		m_block.asm_().movq(xm, r);

		if(g_push128Bits)
		{
			m_block.asm_().pslldq(xm, asmjit::Imm(8));

			m_block.asm_().pop(r.get());

			RegXMM xt(m_block);
			m_block.asm_().movq(xt, r);
			m_block.asm_().movsd(xm, xt);
		}
	}

	PushGPRegs::PushGPRegs(JitBlock& _block)
	: m_r8(_block, asmjit::x86::r8), m_r9(_block, asmjit::x86::r9)
	, m_r10(_block, asmjit::x86::r10), m_r11(_block, asmjit::x86::r11)
	{
	}

	PushBeforeFunctionCall::PushBeforeFunctionCall(JitBlock& _block) : m_xmm(_block) , m_gp(_block), m_shadow(_block)
	{
	}

	DSPReg::DSPReg(JitBlock& _block, JitDspRegPool::DspReg _reg, bool _read, bool _write)
	: m_block(_block), m_dspReg(_reg), m_reg(_block.dspRegPool().get(_reg, _read, _write))
	{
		_block.dspRegPool().lock(_reg);
	}

	DSPReg::~DSPReg()
	{
		m_block.dspRegPool().unlock(m_dspReg);
	}
}
