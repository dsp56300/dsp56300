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
		JitScopedReg(JitBlock& _block, JitRegpool& _pool) : m_block(_block), m_pool(_pool) { acquire(); }	// TODO: move acquire() to (first) get, will greatly reduce register pressure if the RegGP is passed in as parameter
		~JitScopedReg() { release(); }

		JitScopedReg& operator = (const JitScopedReg&) = delete;

		void acquire();

		void release();

	protected:
		asmjit::x86::Reg m_reg;
	private:
		JitBlock& m_block;
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
		PushGP(JitBlock& _block, const JitReg64& _reg, bool _onlyIfUsedInPool = false);
		~PushGP();

		const JitReg64& get() const { return m_reg; }
		operator const JitReg64& () const { return m_reg; }

	private:
		JitBlock& m_block;
		const JitReg64 m_reg;
		const bool m_pushed;
	};

	class FuncArg : PushGP
	{
	public:
		FuncArg(JitBlock& _block, const JitReg64& _reg) : PushGP(_block, _reg, true) {}
	};

	class ShiftReg : public PushGP
	{
	public:
		ShiftReg(JitBlock& _block) : PushGP(_block, asmjit::x86::rcx, true) {}
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

	class PushBeforeFunctionCall
	{
	public:
		PushBeforeFunctionCall(JitBlock& _block);

		PushXMMRegs m_xmm;
		PushGPRegs m_gp;
	};
}
