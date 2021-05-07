#include "jitmem.h"

#include "jitblock.h"
#include "jitregtracker.h"

namespace dsp56k
{
	void Jitmem::mov(TReg24& _dst, const asmjit::x86::Xmm& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().movd(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(TReg48& _dst, const asmjit::x86::Xmm& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().movq(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(TReg56& _dst, const asmjit::x86::Xmm& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().movq(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(const asmjit::x86::Xmm& _dst, TReg24& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().movd(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(const asmjit::x86::Xmm& _dst, TReg48& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().movq(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, TReg24& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().mov(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(TReg24& _dst, const asmjit::x86::Gp& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().mov(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(const asmjit::x86::Xmm& _dst, TReg56& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().movq(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(uint64_t& _dst, const asmjit::x86::Gp& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(uint32_t& _dst, const asmjit::x86::Gp& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, uint64_t& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, uint32_t& _src)
	{
		const RegGP reg(m_block.gpPool());
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	template<typename T>
	asmjit::x86::Mem Jitmem::ptr(const asmjit::x86::Gpq& _temp, const T* _t) const
	{
		if constexpr (sizeof(T*) == sizeof(uint64_t))
			m_block.asm_().mov(_temp, reinterpret_cast<uint64_t>(_t));
		else if constexpr (sizeof(T*) == sizeof(uint32_t))
			m_block.asm_().mov(_temp, reinterpret_cast<uint32_t>(_t));
		else
			static_assert(sizeof(T*) == sizeof(uint64_t) || sizeof(T*) == sizeof(uint32_t), "unknown pointer size");

		return asmjit::x86::ptr(_temp, 0, sizeof(T));
	}
}
