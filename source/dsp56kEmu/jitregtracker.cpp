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

	PushGP::PushGP(JitBlock& _block, const JitReg64& _reg, bool _onlyIfUsedInPool/* = true*/) : m_block(_block), m_reg(_reg), m_pushed(!_onlyIfUsedInPool || _block.dspRegPool().isInUse(_reg))
	{
		if(m_pushed)
			m_block.regUsage().push(_reg);
	}

	PushGP::~PushGP()
	{
		if(m_pushed)
			m_block.regUsage().pop(m_reg);
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

		const auto xm = asmjit::x86::xmm(_xmmIndex);

		_block.regUsage().push(xm);

		if(g_push128Bits)
		{
			const RegGP r(_block);
			_block.asm_().psrldq(xm, asmjit::Imm(8));

			_block.asm_().movq(r, xm);
			
			_block.regUsage().push(r.get());
		}
	}

	PushXMM::~PushXMM()
	{
		if(!m_isLoaded)
			return;

		const auto xm = asmjit::x86::xmm(m_xmmIndex);

		m_block.regUsage().pop(xm);

		if(g_push128Bits)
		{
			const RegGP r(m_block);
			m_block.regUsage().pop(r.get());

			m_block.asm_().pslldq(xm, asmjit::Imm(8));

			RegXMM xt(m_block);
			m_block.asm_().movq(xt, r);
			m_block.asm_().movsd(xm, xt);
		}
	}

	PushXMMRegs::PushXMMRegs(JitBlock& _block): m_xmm0(_block, 0), m_xmm1(_block, 1), m_xmm2(_block, 2),
	                                            m_xmm3(_block, 3), m_xmm4(_block, 4), m_xmm5(_block, 5), m_needsPadding(false), m_block(_block)
	{
		uint32_t numPushed = 0;
		if(m_xmm0.isPushed()) ++numPushed;
		if(m_xmm1.isPushed()) ++numPushed;
		if(m_xmm2.isPushed()) ++numPushed;
		if(m_xmm3.isPushed()) ++numPushed;
		if(m_xmm4.isPushed()) ++numPushed;
		if(m_xmm5.isPushed()) ++numPushed;

		// TODO: should be a global thing, not here but before actually calling the func
		if(numPushed & 1)
		{
			_block.asm_().push(asmjit::Imm(0xc0deba5ec0deba5e));
			m_needsPadding = true;
		}
	}

	PushXMMRegs::~PushXMMRegs()
	{
		if(m_needsPadding)
		{
			const RegGP gp(m_block);
			m_block.asm_().pop(gp);
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

	JitRegpool::JitRegpool(std::initializer_list<asmjit::x86::Reg> _availableRegs)
	{
		for (const auto& r : _availableRegs)
			m_availableRegs.push(r);
	}

	void JitRegpool::put(const asmjit::x86::Reg& _reg)
	{
		m_availableRegs.push(_reg);
	}

	asmjit::x86::Reg JitRegpool::get()
	{
		assert(!m_availableRegs.empty() && "no more temporary registers left");
		const auto ret = m_availableRegs.top();
		m_availableRegs.pop();
		return ret;
	}

	bool JitRegpool::empty() const
	{
		return m_availableRegs.empty();
	}

	void JitScopedReg::acquire()
	{
		if (m_acquired)
			return;
		m_reg = m_pool.get();
		m_block.regUsage().setUsed(m_reg);
		m_acquired = true;
	}

	void JitScopedReg::release()
	{
		if (!m_acquired)
			return;
		m_pool.put(m_reg);
		m_acquired = false;
	}

	RegGP::RegGP(JitBlock& _block) : JitScopedReg(_block, _block.gpPool())
	{
	}

	RegXMM::RegXMM(JitBlock& _block) : JitScopedReg(_block, _block.xmmPool())
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
