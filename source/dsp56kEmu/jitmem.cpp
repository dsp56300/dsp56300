#include "jitmem.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitregtracker.h"

namespace dsp56k
{
	void Jitmem::mov(TReg24& _dst, const JitReg128& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().movd(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(TReg48& _dst, const JitReg128& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().movq(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(TReg56& _dst, const JitReg128& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().movq(ptr(reg.get(), _dst), _src);
	}

	void Jitmem::mov(const JitReg128& _dst, TReg24& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().movd(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(const JitReg128& _dst, TReg48& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().movq(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, TReg24& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(_dst.r32(), ptr(reg.get(), _src));
	}

	void Jitmem::mov(TReg24& _dst, const asmjit::x86::Gp& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(ptr(reg.get(), _dst), _src.r32());
	}

	void Jitmem::mov(const JitReg128& _dst, TReg56& _src)
	{
		const RegGP reg(m_block);
		m_block.asm_().movq(_dst, ptr(reg.get(), _src));
	}

	void Jitmem::mov(uint64_t& _dst, const asmjit::x86::Gp& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(uint32_t& _dst, const asmjit::x86::Gp& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, uint64_t& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, uint32_t& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::getOpWordB(const JitReg& _dst) const
	{
		auto pc = m_block.regs().getPC();

		readDspMemory(_dst, MemArea_P, pc);
		m_block.asm_().inc(pc);
	}

	void Jitmem::readDspMemory(const JitReg& _dst, const EMemArea _area, const JitReg& _offset) const
	{
		const RegGP t(m_block);

		getMemAreaBasePtr(t.get(), _area);

		m_block.asm_().mov(_dst.r32(), asmjit::x86::ptr(t, _offset, 2, 0, sizeof(TWord)));
	}

	void Jitmem::writeDspMemory(const EMemArea _area, const JitReg& _offset, const JitReg& _src) const
	{
		const RegGP t(m_block);

		getMemAreaBasePtr(t.get(), _area);

		m_block.asm_().mov(asmjit::x86::ptr(t, _offset, 2, 0, sizeof(TWord)), _src.r32());
	}

	TWord callDSPMemReadPeriph(DSP* const _dsp, const TWord _area, const TWord _offset)
	{
		return _dsp->getPeriph(_area)->read(_offset);
	}

	void Jitmem::readPeriph(const JitReg64& _dst, const EMemArea _area, const JitReg64& _offset) const
	{
		PushGP r2(m_block, regArg2);
		PushGP rPadding(m_block, regArg2);	// 16 byte alignment

		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(regArg2, _offset);

		m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const
	{
		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area == MemArea_Y ? 1 : 0);

		PushGP r2(m_block, regArg2);
		PushGP r3(m_block, regArg3);

		m_block.asm_().mov(regArg2, _offset);
		m_block.asm_().mov(regArg3, _value);
		m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));
	}

	void Jitmem::getMemAreaBasePtr(const JitReg64& _dst, EMemArea _area) const
	{
		auto& mem = m_block.dsp().memory();

		switch (_area)
		{
		case MemArea_X:		ptrToReg(_dst, mem.x);	break;
		case MemArea_Y:		ptrToReg(_dst, mem.y);	break;
		case MemArea_P:		ptrToReg(_dst, mem.p);	break;
		default:
			assert(0 && "invalid memory area");
			break;
		}
	}

	template<typename T>
	asmjit::x86::Mem Jitmem::ptr(const JitReg64& _temp, const T* _t) const
	{
		ptrToReg<T>(_temp, _t);
		return asmjit::x86::ptr(_temp, 0, sizeof(T));
	}

	template <typename T> void Jitmem::ptrToReg(const JitReg64& _r, const T* _t) const
	{
		if constexpr (sizeof(T*) == sizeof(uint64_t))
			m_block.asm_().mov(_r, reinterpret_cast<uint64_t>(_t));
		else if constexpr (sizeof(T*) == sizeof(uint32_t))
			m_block.asm_().mov(_r, reinterpret_cast<uint32_t>(_t));
		else
			static_assert(sizeof(T*) == sizeof(uint64_t) || sizeof(T*) == sizeof(uint32_t), "unknown pointer size");
	}
}
