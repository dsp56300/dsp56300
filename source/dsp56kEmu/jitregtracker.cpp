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

	AluReg::AluReg(JitBlock& _block, const TWord _aluIndex, bool readOnly/* = false*/, bool writeOnly/* = false*/)
	: m_block(_block)
	, m_reg(_block)
	, m_readOnly(readOnly)
	, m_writeOnly(writeOnly)
	, m_aluIndex(_aluIndex)
	{
		if(!writeOnly)
			m_block.regs().getALU(m_reg, _aluIndex);
	}

	AluReg::~AluReg()
	{
		if(!m_readOnly)
			m_block.regs().setALU(m_aluIndex, m_reg);
	}

	JitReg64 AluReg::get()
	{
		return m_reg.get();
	}

	void AluReg::release()
	{
		m_reg.release();
	}

	AluRef::AluRef(JitBlock& _block, const TWord _aluIndex, const bool _read, const bool _write)
	: m_block(_block)
	, m_read(_read)
	, m_write(_write)
	, m_aluIndex(_aluIndex)
	{
		m_reg.reset();
	}

	AluRef::~AluRef()
	{
		auto& p = m_block.dspRegPool();

		if(p.isParallelOp() && m_write)
		{
			// ALU write registers stay locked
		}
		else
		{
			const auto dspReg = static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA + m_aluIndex);
			p.unlock(dspReg);
		}
	}

	JitReg64 AluRef::get()
	{
		if(m_reg.isValid())
			return m_reg;

		auto& p = m_block.dspRegPool();

		JitReg r;

		const auto dspRegR = static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspA      + m_aluIndex);
		const auto dspRegW = static_cast<JitDspRegPool::DspReg>(JitDspRegPool::DspAwrite + m_aluIndex);

		if(p.isParallelOp() && m_write)
		{
			JitReg rRead;

			if(m_read)
			{
				rRead = p.get(dspRegR, true, false);
				p.lock(dspRegR);
			}

			r = p.get(dspRegW, false, true);

			if(!p.isLocked(dspRegW))
				p.lock(dspRegW);

			if(m_read)
			{
				m_block.asm_().mov(r, rRead);
				p.unlock(dspRegR);
			}
		}
		else
		{
			r = p.get(dspRegR, m_read, m_write);
			p.lock(dspRegR);
		}
		m_reg = r.as<JitReg64>();
		return m_reg;
	}

	AguReg::AguReg(JitBlock& _block, JitDspRegPool::DspReg _regBase, int _aguIndex, bool readOnly) : DSPReg(_block, static_cast<JitDspRegPool::DspReg>(_regBase + _aguIndex), true, !readOnly)
	{
	}

	PushGP::PushGP(JitBlock& _block, const JitReg64& _reg, bool _onlyIfUsedInPool/* = true*/) : m_block(_block), m_reg(_reg), m_pushed(!_onlyIfUsedInPool || _block.dspRegPool().isInUse(_reg))
	{
		if(m_pushed)
			m_block.stack().push(_reg);
	}

	PushGP::~PushGP()
	{
		if(m_pushed)
			m_block.stack().pop(m_reg);
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

		_block.stack().push(xm);

		if(g_push128Bits)
		{
			const RegGP r(_block);
			_block.asm_().psrldq(xm, asmjit::Imm(8));

			_block.asm_().movq(r, xm);
			
			_block.stack().push(r.get());
		}
	}

	PushXMM::~PushXMM()
	{
		if(!m_isLoaded)
			return;

		const auto xm = asmjit::x86::xmm(m_xmmIndex);

		m_block.stack().pop(xm);

		if(g_push128Bits)
		{
			const RegGP r(m_block);
			m_block.stack().pop(r.get());

			m_block.asm_().pslldq(xm, asmjit::Imm(8));

			RegXMM xt(m_block);
			m_block.asm_().movq(xt, r);
			m_block.asm_().movsd(xm, xt);
		}
	}

	PushXMMRegs::PushXMMRegs(JitBlock& _block): m_xmm0(_block, 0), m_xmm1(_block, 1), m_xmm2(_block, 2),
	                                            m_xmm3(_block, 3), m_xmm4(_block, 4), m_xmm5(_block, 5), m_block(_block)
	{
	}

	PushXMMRegs::~PushXMMRegs()
	{
	}

	PushGPRegs::PushGPRegs(JitBlock& _block)
	: m_r8(_block, asmjit::x86::r8, true), m_r9(_block, asmjit::x86::r9, true)
	, m_r10(_block, asmjit::x86::r10, true), m_r11(_block, asmjit::x86::r11, true)
	{
	}

	PushBeforeFunctionCall::PushBeforeFunctionCall(JitBlock& _block) : m_xmm(_block) , m_gp(_block)
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
		m_block.stack().setUsed(m_reg);
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

	void DSPReg::write()
	{
		m_block.dspRegPool().get(m_dspReg, false, true);
	}
}
