#pragma once

#include <list>

#include "jitdspregpool.h"
#include "types.h"
#include "jitregtypes.h"
#include "jithelper.h"

#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	class JitScopedReg;
	class JitBlock;

	class JitRegpool
	{
	public:
		JitRegpool(std::initializer_list<JitReg> _availableRegs);

		void put(JitScopedReg* _scopedReg, bool _weak);
		JitReg get(JitScopedReg* _scopedReg, bool _weak);
		bool empty() const;
		bool isInUse(const JitReg& _gp) const;

		size_t capacity() const { return m_capacity; }

		void reset(std::initializer_list<JitReg> _availableRegs);

	private:
		const size_t m_capacity;
		std::vector<JitReg> m_availableRegs;
		std::list<JitScopedReg*> m_weakRegs;
	};

	class JitScopedReg
	{
	public:
		JitScopedReg() = delete;
		JitScopedReg(const JitScopedReg&) = delete;
		JitScopedReg(JitScopedReg&& _other) noexcept : m_reg(_other.m_reg), m_block(_other.m_block), m_pool(_other.m_pool), m_acquired(_other.m_acquired), m_weak(_other.m_weak)
		{
			_other.m_acquired = false;
		}
		JitScopedReg(JitBlock& _block, JitRegpool& _pool, const bool _acquire = true, const bool _weak = false) : m_block(_block), m_pool(_pool), m_weak(_weak)
		{
			if(_acquire) 
				acquire();
		}
		~JitScopedReg()
		{
			release();
		}

		JitScopedReg& operator = (const JitScopedReg&) = delete;

		JitScopedReg& operator = (JitScopedReg&& _other) noexcept;

		void acquire();

		void release();

		bool isValid() const { return m_acquired; }
		bool isWeak() const { return m_weak; }

		const JitReg& get() const { assert(isValid()); return m_reg; }

		JitBlock& block() { return m_block; }

	private:
		JitReg m_reg;
		JitBlock& m_block;
		JitRegpool& m_pool;
		bool m_acquired = false;
		bool m_weak = false;
	};

	class RegGP : public JitScopedReg
	{
	public:
		RegGP(JitBlock& _block, bool _acquire = true, bool _weak = false);
		RegGP(RegGP&& _other) noexcept : JitScopedReg(std::move(_other)) {}

		const JitReg64& get() const { return JitScopedReg::get().as<JitReg64>(); }
		operator const JitReg64& () const { return get(); }
		RegGP& operator = (RegGP&& other) noexcept
		{
			JitScopedReg::operator=(std::move(other));
			return *this;
		}
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
		DSPReg(JitBlock& _block, JitDspRegPool::DspReg _reg, bool _read = true, bool _write = true, bool _acquire = true);
		DSPReg(DSPReg&& _other) noexcept;
		~DSPReg();

		JitRegGP get() const { return m_reg; }
		operator JitRegGP() const { return get(); }

		JitReg32 r32() const { return dsp56k::r32(get()); }
		JitReg64 r64() const { return dsp56k::r64(get()); }

		void write();

		operator asmjit::Imm () const = delete;

		void acquire();
		void release();

		bool acquired() const { return m_acquired; }
		bool read() const { return m_read; }
		bool write() const { return m_write; }

		JitDspRegPool::DspReg dspReg() const { return m_dspReg; }

		JitBlock& block() { return m_block; }

		DSPReg& operator = (const DSPReg& _other) = delete;
		DSPReg& operator = (DSPReg&& _other) noexcept;

	private:
		JitBlock& m_block;
		bool m_read;
		bool m_write;
		bool m_acquired;
		bool m_locked;
		JitDspRegPool::DspReg m_dspReg;
		JitRegGP m_reg;
	};

	class DSPRegTemp
	{
	public:
		DSPRegTemp(JitBlock& _block, bool _acquire);

		DSPRegTemp(const DSPRegTemp& _existing) = delete;
		DSPRegTemp(DSPRegTemp&& _existing) noexcept : m_block(_existing.m_block)
		{
			*this = std::move(_existing);
		}

		~DSPRegTemp();

		JitReg64 get() const { return r64(m_reg); }
		operator JitReg64() const { return get(); }

		void acquire();
		void release();

		bool acquired() const { return m_dspReg != JitDspRegPool::DspRegInvalid; }

		DSPRegTemp& operator = (const DSPRegTemp& _other) = delete;
		DSPRegTemp& operator = (DSPRegTemp&& _other) noexcept
		{
			m_reg = _other.m_reg;
			m_dspReg = _other.m_dspReg;

			_other.m_reg.reset();
			_other.m_dspReg = JitDspRegPool::DspRegInvalid;

			return *this;
		}

	private:
		JitBlock& m_block;
		JitDspRegPool::DspReg m_dspReg = JitDspRegPool::DspRegInvalid;
		JitRegGP m_reg;
	};

	class AluReg
	{
	public:
		AluReg(JitBlock& _block, TWord _aluIndex, bool readOnly = false, bool writeOnly = false);
		~AluReg();
		JitReg64 get() const;
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

	class PushGP
	{
	public:
		PushGP(JitBlock& _block, const JitReg64& _reg);
		~PushGP();

		const JitReg64& get() const { return m_reg; }
		operator const JitReg64& () const { return m_reg; }

	protected:
		JitBlock& m_block;
	private:
		const JitReg64 m_reg;
		const bool m_pushed;
	};

	class FuncArg : public PushGP
	{
	public:
		FuncArg(JitBlock& _block, const uint32_t& _argIndex);
		~FuncArg();

	private:
		const uint32_t m_funcArgIndex;
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
		PushGPRegs(JitBlock& _block, bool _isJitCall);
		~PushGPRegs();
	private:
		JitBlock& m_block;
		std::list<JitRegGP> m_pushedRegs;
	};

	class PushBeforeFunctionCall
	{
	public:
		PushBeforeFunctionCall(JitBlock& _block, bool _isJitCall);

		PushXMMRegs m_xmm;
		PushGPRegs m_gp;
	};

	class RegScratch
	{
	public:
		explicit RegScratch(JitBlock& _block, bool _acquire = true);
		RegScratch(RegScratch&& _other) noexcept : m_block(_other.m_block)
		{
			if(_other.isValid())
			{
				_other.release();
				acquire();
			}
		}
		explicit RegScratch(const RegScratch&) = delete;

		~RegScratch();

		void acquire();
		void release();
		
		RegScratch& operator = (RegScratch&& _other) noexcept
		{
			if(_other.isValid())
			{
				_other.release();
				acquire();
			}
			return *this;
		}
		RegScratch& operator = (const RegScratch&) = delete;

		JitReg64 get() const { assert(isValid()); return isValid() ? regReturnVal : JitReg64(); }
		JitReg64 reg() const { return get(); }
		operator JitReg64 () const { return get(); }

		bool isValid() const
		{
			return m_acquired;
		}

#ifdef HAVE_X86_64
		auto r8() const { return get().r8(); }
#endif

	private:
		JitBlock& m_block;
		bool m_acquired = false;
	};
}
