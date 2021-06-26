#pragma once

#include <stack>

#include "jitdspregpool.h"
#include "types.h"
#include "jitregtypes.h"

#include "asmjit/x86/x86operand.h"

namespace asmjit
{
	namespace x86
	{
		class Assembler;
	}
}

namespace dsp56k
{
	class JitBlock;

	class JitRegpool
	{
	public:
		JitRegpool(std::initializer_list<asmjit::x86::Reg> _availableRegs);

		void put(const asmjit::x86::Reg& _reg);
		asmjit::x86::Reg get();
		bool empty() const;

	private:
		std::stack<asmjit::x86::Reg> m_availableRegs;	// TODO: do we want a FIFO instead to have more register spread? Is it any better performance-wise?
	};

	class JitScopedReg
	{
	public:
		JitScopedReg() = delete;
		JitScopedReg(const JitScopedReg&) = delete;
		JitScopedReg(JitRegpool& _pool) : m_pool(_pool), m_reg({}) { acquire(); }	// TODO: move acquire() to (first) get, will greatly reduce register pressure if the RegGP is passed in as parameter
		~JitScopedReg() { release(); }

		JitScopedReg& operator = (const JitScopedReg&) = delete;

		void acquire()
		{
			if(m_acquired)
				return;
			m_reg = m_pool.get();
			m_acquired = true;
		}
		void release()
		{
			if(!m_acquired)
				return;
			m_pool.put(m_reg);
			m_acquired = false;
		}

	protected:
		asmjit::x86::Reg m_reg;
	private:
		JitRegpool& m_pool;
		bool m_acquired = false;
	};

	class RegGP : public JitScopedReg
	{
	public:
		RegGP(JitBlock& _block);

		const JitReg64& get() const { return m_reg.as<JitReg64>(); }
		operator const JitReg64& () const { return get(); }
	};
	
	class RegXMM : public JitScopedReg
	{
	public:
		RegXMM(JitBlock& _block);

		const JitReg128& get() const { return m_reg.as<JitReg128>(); }
		operator const JitReg128& () const { return get(); }
	};

	class DSPReg
	{
	public:
		DSPReg(JitBlock& _block, JitDspRegPool::DspReg _reg, bool _read = true, bool _write = true);
		~DSPReg();

		JitReg get() const { return m_reg; }
		operator JitReg() const { return get(); }

		JitReg32 r32() const { return get().r32(); }
		JitReg64 r64() const { return get().r64(); }

		operator asmjit::Imm () const = delete;

	private:
		JitBlock& m_block;
		const JitDspRegPool::DspReg m_dspReg;
		const JitReg m_reg;
	};
	
	class DSPRegTemp
	{
	public:
		DSPRegTemp(JitBlock& _block);
		~DSPRegTemp();

		JitReg64 get() const { return m_reg.r64(); }
		operator JitReg64() const { return get(); }

		void acquire();
		void release();

		bool acquired() const { return m_dspReg != JitDspRegPool::DspCount; }

	private:
		JitBlock& m_block;
		JitDspRegPool::DspReg m_dspReg = JitDspRegPool::DspCount;
		JitReg m_reg;
	};

	class AluReg
	{
	public:
		AluReg(JitBlock& _block, TWord _aluIndexSrc, bool readOnly = false, bool writeOnly = false, TWord _aluIndexDst = ~0);
		~AluReg();
		JitReg64 get() const { return m_reg.get(); }
		operator JitReg64 () const { return get(); }
		operator const RegGP& () const { return m_reg; }
		void release();

	private:
		JitBlock& m_block;
		RegGP m_reg;
		const TWord m_aluIndexDst;
		const bool m_readOnly;
	};

	class AguReg : public DSPReg
	{
	public:
		AguReg(JitBlock& _block, JitDspRegPool::DspReg _regBase, int _aguIndex, bool readOnly = false);
	};

	class AguRegM : public AguReg
	{
	public:
		AguRegM(JitBlock& _block, int _aguIndex, bool readOnly = true) : AguReg(_block, JitDspRegPool::DspM0, _aguIndex, readOnly)
		{
		}
	};

	class AguRegN : public AguReg
	{
	public:
		AguRegN(JitBlock& _block, int _aguIndex, bool readOnly = true) : AguReg(_block, JitDspRegPool::DspN0, _aguIndex, readOnly)
		{
		}
	};

	class PushGP
	{
	public:
		PushGP(asmjit::x86::Assembler& _a, const JitReg64& _reg);
		~PushGP();

		const JitReg64& get() const { return m_reg; }
		operator const JitReg64& () const { return m_reg; }

	private:
		asmjit::x86::Assembler& m_asm;
		const JitReg64 m_reg;
	};

	class PushExchange
	{
	public:
		PushExchange(asmjit::x86::Assembler& _a, const JitReg64& _regA, const JitReg64& _regB);
		~PushExchange();

	private:
		void swap() const;

		asmjit::x86::Assembler& m_asm;
		const JitReg64 m_regA;
		const JitReg64 m_regB;
	};

	class PushShadowSpace
	{
	public:
		PushShadowSpace(JitBlock& _block);
		~PushShadowSpace();
	private:
		JitBlock& m_block;
	};

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
		PushXMM m_xmm0;
		PushXMM m_xmm1;
		PushXMM m_xmm2;
		PushXMM m_xmm3;
		PushXMM m_xmm4;
		PushXMM m_xmm5;
		bool m_needsPadding;
		JitBlock& m_block;
	};

	class PushGPRegs
	{
	public:
		PushGPRegs(JitBlock& _block);
	private:
		PushGP m_r8;
		PushGP m_r9;
		PushGP m_r10;
		PushGP m_r11;
	};

	class PushNonVolatiles
	{
	public:
		explicit PushNonVolatiles(asmjit::x86::Assembler& _a)
		: m_tempA(_a, regGPTempA)
		, m_tempB(_a, regGPTempB)
		, m_tempC(_a, regGPTempC)
		, m_tempD(_a, regGPTempD)
		, m_tempE(_a, regGPTempE)
		, m_rbx(_a, asmjit::x86::rbx)
		, m_rsi(_a, asmjit::x86::rsi)
		, m_rdi(_a, asmjit::x86::rdi)
		{
		}

		PushGP m_tempA;
		PushGP m_tempB;
		PushGP m_tempC;
		PushGP m_tempD;
		PushGP m_tempE;
		PushGP m_rbx;
		PushGP m_rsi;
		PushGP m_rdi;
	};

	class PushBeforeFunctionCall
	{
	public:
		PushBeforeFunctionCall(JitBlock& _block);

		PushXMMRegs m_xmm;
		PushGPRegs m_gp;
		PushShadowSpace m_shadow;
	};
}
