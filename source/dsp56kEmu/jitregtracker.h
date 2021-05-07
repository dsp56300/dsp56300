#pragma once

#include "assert.h"

#include <stack>


#include "types.h"
#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	class JitBlock;

	template<typename T>
	class JitRegpool
	{
	public:
		JitRegpool(std::initializer_list<T> _availableRegs)
		{
			for (auto allowedReg : _availableRegs)
				m_availableRegs.push(allowedReg);
		}

		void put(const T& _reg)
		{
			m_availableRegs.push(_reg);
		}

		T get()
		{
			assert(!m_availableRegs.empty());
			const auto ret = m_availableRegs.top();
			m_availableRegs.pop();
			return ret;
		}
	private:
		std::stack<T> m_availableRegs;
	};

	template<typename T>
	class JitScopedReg
	{
	public:
		JitScopedReg() = delete;
		JitScopedReg(JitRegpool<T>& _pool) : m_pool(_pool), m_reg(m_pool.get()) {}
		~JitScopedReg() { release(); }

		const T& get() const { return m_reg; }

		operator const T& () const { return get(); }

		template<typename U = T>
		operator typename std::enable_if_t<std::is_same<U, asmjit::x86::Gpq>::value, asmjit::x86::Gp>::type () const
		{
			return get();
		}

		template<typename U = T> operator typename std::enable_if_t<std::is_same<U, asmjit::x86::Gpq>::value, asmjit::Imm>::type () const = delete;

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
		bool m_acquired = true;
	};

	using RegGP = JitScopedReg<asmjit::x86::Gpq>;
	using RegXMM = JitScopedReg<asmjit::x86::Xmm>;

	class AluReg
	{
	public:
		AluReg(JitBlock& _block, TWord _aluIndex, bool readOnly = false);
		~AluReg();
		asmjit::x86::Gpq get() const { return m_reg.get(); }
		operator asmjit::x86::Gpq () const { return m_reg.get(); }
		operator const RegGP& () const { return m_reg; }
	private:
		JitBlock& m_block;
		const RegGP m_reg;
		const TWord m_aluIndex;
		const bool m_readOnly;
	};
}