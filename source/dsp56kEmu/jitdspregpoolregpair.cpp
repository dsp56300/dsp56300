#include "jitdspregpoolregpair.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitemitter.h"

namespace dsp56k
{
	void JitRegPoolRegPair::get(const JitReg64& _dst) const
	{
		// If one of the partial registers is written, we have to update our combined reg
		const auto w0 = m_pool.isWritten(m_reg0);
		const auto w1 = m_pool.isWritten(m_reg1);
		if (w0 || w1)
		{
			const DSPReg regBase(getBlock(), m_regBase, !w0 || !w1, false);

			if(w0 && w1)
			{
				const DSPReg reg0(getBlock(), m_reg0, true, false);
				const DSPReg reg1(getBlock(), m_reg1, true, false);

				merge(r64(regBase), r32(reg1), r32(reg0));
			}
			else if(w0)
			{
				const DSPReg reg0(getBlock(), m_reg0, true, false);

				replaceLow(r64(regBase), r32(reg0));
			}
			else
			{
				const DSPReg reg1(getBlock(), m_reg1, true, false);

				// use dst as temp
				replaceHigh(_dst, r64(regBase), r32(reg1));
			}
			m_pool.clearWritten(m_reg0);
			m_pool.clearWritten(m_reg1);

			asm_().mov(r64(_dst), r64(regBase));
		}
		// If the common reg is already loaded, use it
		else if (m_pool.isInUse(m_regBase))
		{  // NOLINT(bugprone-branch-clone)
			m_pool.read(_dst, m_regBase);
		}
		// Can we combine our two partial registers to prevent loading from memory?
		else if (m_pool.isInUse(m_reg0) && m_pool.isInUse(m_reg1))
		{
			// Yes
			// TODO: Do we want to update the common register to keep it in pool or not?
			merge(_dst, r32(m_pool.get(m_reg1, true, false)), r32(m_pool.get(m_reg0, true, false)));
		}
		else
		{
			m_pool.read(_dst, m_regBase);
		}
	}

	void JitRegPoolRegPair::get0(const JitReg32& _dst) const
	{
		mov(_dst, m_reg0);
	}

	void JitRegPoolRegPair::get1(const JitReg32& _dst) const
	{
		mov(_dst, m_reg1);
	}

	DspValue JitRegPoolRegPair::get0(bool _read, bool _write) const
	{
		return DspValue(getBlock(), m_reg0, _read, _write);
	}

	DspValue JitRegPoolRegPair::get1(bool _read, bool _write) const
	{
		return DspValue(getBlock(), m_reg1, _read, _write);
	}

	void JitRegPoolRegPair::set0(const DspValue& _src) const
	{
		m_pool.write(m_reg0, _src);
	}

	void JitRegPoolRegPair::set1(const DspValue& _src) const
	{
		m_pool.write(m_reg1, _src);
	}

	void JitRegPoolRegPair::store0(const JitReg32& _src) const
	{
		assert(m_pool.isWritten(m_reg0));

		// if both partials are written, store them together
		if(m_pool.isWritten(m_reg0) && m_pool.isWritten(m_reg1))
		{
			store01(_src, r32(m_pool.get(m_reg1, true, false)));
			return;
		}

		const RegGP s(getBlock());
		m_pool.movDspReg(r64(s), dspReg());
		replaceLow(r64(s), _src);
		m_pool.movDspReg(dspReg(), r64(s));
	}

	void JitRegPoolRegPair::store1(const JitReg32& _src) const
	{
		assert(m_pool.isWritten(m_reg1));

		// if both partials are written, store them together
		if (m_pool.isWritten(m_reg0) && m_pool.isWritten(m_reg1))
		{
			store01(r32(m_pool.get(m_reg0, true, false)), _src);
			return;
		}

		const RegGP s(getBlock());
		m_pool.movDspReg(r64(s), dspReg());
		replaceHigh(r64(_src), r64(s), _src);	// note: we are destroying our src register here, but as it is stored when released it doesn't matter
		m_pool.movDspReg(dspReg(), r64(s));
	}

	void JitRegPoolRegPair::load0(const JitReg32& _dst) const
	{
		// If the common register is available, extract the part we need from it
		if (m_pool.isInUse(m_regBase))
		{
			const DSPReg xyRef(getBlock(), m_regBase, true, false);
			extractLow(_dst, xyRef);
		}
		else
		{
			// load from memory
			m_pool.movDspReg(r64(_dst), dspReg());
			asm_().and_(r32(_dst), asmjit::Imm(0xffffff));
		}
	}

	void JitRegPoolRegPair::load1(const JitReg32& _dst) const
	{
		// If the common register is available, extract the part we need from it
		if (m_pool.isInUse(m_regBase))
		{
			const DSPReg xyRef(getBlock(), m_regBase, true, false);
			extractHigh(_dst, xyRef);
		}
		else
		{
			// load from memory
			m_pool.movDspReg(r64(_dst), dspReg());
			asm_().shr(r64(_dst), asmjit::Imm(24));
		}
	}

	void JitRegPoolRegPair::store01(const JitReg32& _reg0, const JitReg32& _reg1) const
	{
		assert(m_pool.isWritten(m_reg0) && m_pool.isWritten(m_reg1));

		const RegGP s(getBlock());

		merge(r64(s), _reg1, _reg0);

		m_pool.movDspReg(dspReg(), r64(s));

		m_pool.clearWritten(m_reg0);
		m_pool.clearWritten(m_reg1);
	}
#ifdef HAVE_ARM64
	void JitRegPoolRegPair::merge(const JitReg64& _dst, const JitReg32& _reg1, const JitReg32& _reg0) const
	{
		asm_().lsl(_dst, r64(_reg1), asmjit::Imm(24));
		asm_().bfi(_dst, r64(_reg0), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitRegPoolRegPair::replaceLow(const JitReg64& _dst, const JitReg32& _src) const
	{
		asm_().bfi(_dst, r64(_src), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitRegPoolRegPair::replaceHigh(const JitReg64&, const JitReg64& _dst, const JitReg32& _src) const
	{
		asm_().bfi(_dst, r64(_src), asmjit::Imm(24), asmjit::Imm(24));
	}

	void JitRegPoolRegPair::extractLow(const JitReg32& _dst, const JitRegGP& _src) const
	{
		asm_().ubfx(r64(_dst), r64(_src), asmjit::Imm(0), asmjit::Imm(24));
	}

	void JitRegPoolRegPair::extractHigh(const JitReg32& _dst, const JitRegGP& _src) const
	{
		asm_().ubfx(r64(_dst), r64(_src), asmjit::Imm(24), asmjit::Imm(24));
	}
#else
	void JitRegPoolRegPair::merge(const JitReg64& _dst, const JitReg32& _reg1, const JitReg32& _reg0) const
	{
		asm_().rol(r64(_dst), r64(_reg1), 24);
		asm_().or_(r64(_dst), r64(_reg0));
	}

	void JitRegPoolRegPair::replaceLow(const JitReg64& _dst, const JitReg32& _src) const
	{
		asm_().and_(r64(_dst), asmjit::Imm(0xffffffffff000000));
		asm_().or_(r64(_dst), r64(_src));
	}

	void JitRegPoolRegPair::replaceHigh(const JitReg64& _temp, const JitReg64& _dst, const JitReg32& _src) const
	{
		if(r32(_temp) == _src)
			asm_().shl(r64(_src), 24);
		else
			asm_().rol(r64(_temp), r64(_src), 24);

		asm_().and_(r32(_dst), asmjit::Imm(0xffffff));
		asm_().or_(r64(_dst), r64(_temp));
	}

	void JitRegPoolRegPair::extractLow(const JitReg32& _dst, const JitRegGP& _src) const
	{
		asm_().mov(r32(_dst), r32(_src));
		asm_().and_(r32(_dst), asmjit::Imm(0xffffff));
	}

	void JitRegPoolRegPair::extractHigh(const JitReg32& _dst, const JitRegGP& _src) const
	{
		asm_().mov(r64(_dst), r64(_src));
		asm_().shr(r64(_dst), asmjit::Imm(24));
	}
#endif
	void JitRegPoolRegPair::mov(const JitReg32& _dst, const JitRegGP& _src) const
	{
		asm_().mov(r32(_dst), r32(_src));
	}

	void JitRegPoolRegPair::mov(const JitReg32& _dst, const PoolReg _src) const
	{
		mov(_dst, r32(m_pool.get(_src, true, false)));
	}

	const TReg48& JitRegPoolRegPair::dspReg() const
	{
		const auto& r = getBlock().dsp().regs();
		return m_regBase == DspY ? r.y : r.x;
	}

	JitBlock& JitRegPoolRegPair::getBlock() const
	{
		return m_pool.getBlock();
	}

	JitEmitter& JitRegPoolRegPair::asm_() const
	{
		return getBlock().asm_();
	}
}
