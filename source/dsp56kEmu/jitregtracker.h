#pragma once

#include <stack>

#include "assert.h"
#include "types.h"

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

	template<typename T>
	class JitRegpool
	{
	public:
		JitRegpool(std::initializer_list<T> _availableRegs)
		{
			for (const auto& r : _availableRegs)
				m_availableRegs.push(r);
		}

		void put(const T& _reg)
		{
			m_availableRegs.push(_reg);
		}

		T get()
		{
			assert(!m_availableRegs.empty() && "no more temporary registers left");
			const auto ret = m_availableRegs.top();
			m_availableRegs.pop();
			return ret;
		}
	private:
		std::stack<T> m_availableRegs;	// TODO: do we want a FIFO instead to have more register spread? Is it any better performance-wise?
	};

	template<typename T>
	class JitScopedReg
	{
	public:
		JitScopedReg() = delete;
		JitScopedReg(const JitScopedReg&) = delete;
		JitScopedReg(JitRegpool<T>& _pool) : m_pool(_pool), m_reg({}) { acquire(); }
		~JitScopedReg() { release(); }

		const T& get() const { return m_reg; }

		operator const T& () const { return get(); }

		template<typename U = T>
		operator typename std::enable_if_t<std::is_same<U, asmjit::x86::Gpq>::value, asmjit::x86::Gp>::type () const
		{
			return get();
		}

		template<typename U = T> operator typename std::enable_if_t<std::is_same<U, asmjit::x86::Gpq>::value, asmjit::Imm>::type () const = delete;

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
	private:
		JitRegpool<T>& m_pool;
		T m_reg;
		bool m_acquired = false;
	};

	using RegGP = JitScopedReg<asmjit::x86::Gpq>;
	using RegXMM = JitScopedReg<asmjit::x86::Xmm>;

	class AluReg
	{
	public:
		AluReg(JitBlock& _block, TWord _aluIndexSrc, bool readOnly = false, TWord _aluIndexDst = ~0);
		~AluReg();
		asmjit::x86::Gpq get() const { return m_reg.get(); }
		operator asmjit::x86::Gpq () const { return m_reg.get(); }
		operator const RegGP& () const { return m_reg; }

	private:
		JitBlock& m_block;
		const RegGP m_reg;
		const TWord m_aluIndexDst;
		const bool m_readOnly;
	};

	class PushGP
	{
	public:
		PushGP(asmjit::x86::Assembler& _a, const asmjit::x86::Gpq& _reg);
		~PushGP();

		asmjit::x86::Gpq get() const { return m_reg; }
		operator asmjit::x86::Gpq () const { return m_reg; }

	private:
		asmjit::x86::Assembler& m_asm;
		const asmjit::x86::Gpq m_reg;
	};

	class PushExchange
	{
	public:
		PushExchange(asmjit::x86::Assembler& _a, const asmjit::x86::Gpq& _regA, const asmjit::x86::Gpq& _regB);
		~PushExchange();

	private:
		void swap() const;

		asmjit::x86::Assembler& m_asm;
		const asmjit::x86::Gpq m_regA;
		const asmjit::x86::Gpq m_regB;
	};

}
