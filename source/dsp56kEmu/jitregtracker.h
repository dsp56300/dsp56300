#pragma once

#include <list>

#include "jitdspregpool.h"
#include "types.h"
#include "jitregtypes.h"
#include "jithelper.h"

#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	class JitBlock;

	class JitRegpool
	{
	public:
		JitRegpool(std::initializer_list<JitReg> _availableRegs);

		void put(const JitReg& _reg);
		JitReg get();
		bool empty() const;
		bool isInUse(const JitReg& _gp) const;

	private:
		std::list<JitReg> m_availableRegs;
	};

	class JitScopedReg
	{
	public:
		JitScopedReg() = delete;
		JitScopedReg(const JitScopedReg&) = delete;
		JitScopedReg(JitScopedReg&& _other) noexcept : m_reg(_other.m_reg), m_block(_other.m_block), m_pool(_other.m_pool), m_acquired(_other.m_acquired)
		{
			_other.m_acquired = false;
		}
		JitScopedReg(JitBlock& _block, JitRegpool& _pool, const bool _acquire = true) : m_block(_block), m_pool(_pool)
		{
			if(_acquire) 
				acquire();
		}
		~JitScopedReg()
		{
			release();
		}

		JitScopedReg& operator = (const JitScopedReg&) = delete;

		JitScopedReg& operator = (JitScopedReg&& _other) noexcept
		{
			m_reg = _other.m_reg;
			m_acquired = _other.m_acquired;
			_other.m_acquired = false;
			return *this;
		}

		void acquire();

		void release();

		bool isValid() const { return m_acquired; }

		const JitReg& get() const { assert(isValid()); return m_reg; }
	private:
		JitReg m_reg;
		JitBlock& m_block;
		JitRegpool& m_pool;
		bool m_acquired = false;
	};

	class RegGP : public JitScopedReg
	{
	public:
		RegGP(JitBlock& _block, bool _acquire = true);

		const JitReg64& get() const { return JitScopedReg::get().as<JitReg64>(); }
		operator const JitReg64& () const { return get(); }
	};
	
	class RegXMM : public JitScopedReg
	{
	public:
		RegXMM(JitBlock& _block, bool _acquire = true);

		const JitReg128& get() const { return JitScopedReg::get().as<JitReg128>(); }
		operator const JitReg128& () const { return get(); }
	};

	class DSPReg
	{
	public:
		DSPReg(JitBlock& _block, JitDspRegPool::DspReg _reg, bool _read = true, bool _write = true);
		~DSPReg();

		JitRegGP get() const { return m_reg; }
		operator JitRegGP() const { return get(); }

		JitReg32 r32() const { return dsp56k::r32(get()); }
		JitReg64 r64() const { return dsp56k::r64(get()); }

		void write();

		operator asmjit::Imm () const = delete;

	private:
		JitBlock& m_block;
		const JitDspRegPool::DspReg m_dspReg;
		const JitRegGP m_reg;
	};
	
	class DSPRegTemp
	{
	public:
		DSPRegTemp(JitBlock& _block);
		~DSPRegTemp();

		JitReg64 get() const { return r64(m_reg); }
		operator JitReg64() const { return get(); }

		void acquire();
		void release();

		bool acquired() const { return m_dspReg != JitDspRegPool::DspCount; }

	private:
		JitBlock& m_block;
		JitDspRegPool::DspReg m_dspReg = JitDspRegPool::DspCount;
		JitRegGP m_reg;
	};

	class AluReg
	{
	public:
		AluReg(JitBlock& _block, TWord _aluIndex, bool readOnly = false, bool writeOnly = false);
		~AluReg();
		JitReg64 get();
		operator JitReg64 () { return get(); }
		void release();

	private:
		JitBlock& m_block;
		RegGP m_reg;
		const bool m_readOnly;
		const bool m_writeOnly;
		const TWord m_aluIndex;
	};


	class AluRef
	{
	public:
		AluRef(JitBlock& _block, TWord _aluIndex, bool _read = true, bool _write = true);
		~AluRef();
		JitReg64 get();
		operator JitReg64 () { return get(); }

	private:
		JitBlock& m_block;
		JitReg64 m_reg;
		const bool m_read;
		const bool m_write;
		const TWord m_aluIndex;
	};

	class AguReg : public DSPReg
	{
	public:
		AguReg(JitBlock& _block, JitDspRegPool::DspReg _regBase, int _aguIndex, bool readOnly = false, bool writeOnly = false);
	};

	class AguRegR : public AguReg
	{
	public:
		AguRegR(JitBlock& _block, int _aguIndex, bool readOnly = true) : AguReg(_block, JitDspRegPool::DspR0, _aguIndex, readOnly) {}
	};

	class AguRegN : public AguReg
	{
	public:
		AguRegN(JitBlock& _block, int _aguIndex, bool readOnly = true) : AguReg(_block, JitDspRegPool::DspN0, _aguIndex, readOnly) {}
	};

	class AguRegM : public AguReg
	{
	public:
		AguRegM(JitBlock& _block, int _aguIndex, bool readOnly = true) : AguReg(_block, JitDspRegPool::DspM0, _aguIndex, readOnly) {}
	};

	class AguRegMmod : public AguReg
	{
	public:
		AguRegMmod(JitBlock& _block, int _aguIndex, bool readOnly = true, bool writeOnly = false) : AguReg(_block, JitDspRegPool::DspM0mod, _aguIndex, readOnly, writeOnly) {}
	};

	class AguRegMmask : public AguReg
	{
	public:
		AguRegMmask(JitBlock& _block, int _aguIndex, bool readOnly = true, bool writeOnly = false) : AguReg(_block, JitDspRegPool::DspM0mask, _aguIndex, readOnly, writeOnly) {}
	};

	class PushGP
	{
	public:
		PushGP(JitBlock& _block, const JitReg64& _reg);
		~PushGP();

		const JitReg64& get() const { return m_reg; }
		operator const JitReg64& () const { return m_reg; }

	private:
		JitBlock& m_block;
		const JitReg64 m_reg;
		const bool m_pushed;
	};

	class FuncArg : public PushGP
	{
	public:
		FuncArg(JitBlock& _block, const uint32_t& _argIndex) : PushGP(_block, g_funcArgGPs[_argIndex]) {}
	};

#ifdef HAVE_X86_64
	class ShiftReg : public PushGP
	{
	public:
		ShiftReg(JitBlock& _block) : PushGP(_block, asmjit::x86::rcx) {}
	};
#else
	using ShiftReg = RegGP;
#endif

	class PushXMM
	{
	public:
		PushXMM(JitBlock& _block, uint32_t _xmmIndex);
		~PushXMM();
		bool isPushed() const { return m_isLoaded; }

	private:
		JitBlock& m_block;
		uint32_t m_xmmIndex;
		bool m_isLoaded;
	};

	class PushXMMRegs
	{
	public:
		PushXMMRegs(JitBlock& _block);
		~PushXMMRegs();

	private:
		JitBlock& m_block;
		std::list<JitReg128> m_pushedRegs;
	};

	class PushGPRegs
	{
	public:
		PushGPRegs(JitBlock& _block);
		~PushGPRegs();
	private:
		JitBlock& m_block;
		std::list<JitRegGP> m_pushedRegs;
	};

	class PushBeforeFunctionCall
	{
	public:
		PushBeforeFunctionCall(JitBlock& _block);

		PushXMMRegs m_xmm;
		PushGPRegs m_gp;
	};
}
