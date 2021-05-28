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

	void Jitmem::mov(uint8_t& _dst, const asmjit::x86::Gp& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const uint64_t& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const uint32_t& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const uint8_t& _src) const
	{
		const RegGP reg(m_block);
		m_block.asm_().movzx(_dst, ptr(reg, &_src));
	}

	void Jitmem::readDspMemory(const JitReg& _dst, const EMemArea _area, const JitReg& _offset) const
	{
		const RegGP t(m_block);
		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(_dst.r32(), asmjit::x86::ptr(t, _offset, 2, 0, sizeof(TWord)));
	}

	void Jitmem::writeDspMemory(const EMemArea _area, const JitReg& _offset, const JitReg& _src) const
	{
		const RegGP t(m_block);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(asmjit::x86::ptr(t, _offset, 2, 0, sizeof(TWord)), _src.r32());
	}

	void Jitmem::readDspMemory(const JitReg& _dst, EMemArea _area, TWord _offset) const
	{
		const RegGP t(m_block);

		m_block.dsp().memory().memTranslateAddress(_area, _offset);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(_dst.r32(), asmjit::x86::ptr(t));
	}

	void Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const JitReg& _src) const
	{
		const RegGP t(m_block);

		m_block.dsp().memory().memTranslateAddress(_area, _offset);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(asmjit::x86::ptr(t), _src.r32());
	}

	TWord callDSPMemReadPeriph(DSP* const _dsp, const TWord _area, const TWord _offset)
	{
		return _dsp->getPeriph(_area)->read(_offset);
	}

	void callDSPMemWritePeriph(DSP* const _dsp, const TWord _area, const TWord _offset, const TWord _value)
	{
		_dsp->getPeriph(_area)->write(_offset, _value);
	}

	void Jitmem::readPeriph(const JitReg64& _dst, EMemArea _area, const TWord& _offset) const
	{
		PushGP r2(m_block, regArg2);

		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(regArg2, asmjit::Imm(_offset));

		{
			PushXMMRegs xmms(m_block);
			PushShadowSpace ss(m_block);
			m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));
		}

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::readPeriph(const JitReg64& _dst, const EMemArea _area, const JitReg64& _offset) const
	{
		PushGP r2(m_block, regArg2);

		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(regArg2, _offset);

		{
			PushXMMRegs xmms(m_block);
			PushShadowSpace ss(m_block);
			m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));
		}

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const
	{
		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area == MemArea_Y ? 1 : 0);

		PushGP r2(m_block, regArg2);
		PushGP r3(m_block, regArg3);
		PushGP rPadding(m_block, regArg3);

		m_block.asm_().mov(regArg2, _offset);
		m_block.asm_().mov(regArg3, _value);

		{
			PushXMMRegs xmms(m_block);
			PushShadowSpace ss(m_block);
			m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
		}
	}

	void Jitmem::writePeriph(EMemArea _area, const TWord& _offset, const JitReg64& _value) const
	{
		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area == MemArea_Y ? 1 : 0);

		PushGP r2(m_block, regArg2);
		PushGP r3(m_block, regArg3);
		PushGP rPadding(m_block, regArg3);

		m_block.asm_().mov(regArg2, asmjit::Imm(_offset));
		m_block.asm_().mov(regArg3, _value);

		{
			PushXMMRegs xmms(m_block);
			PushShadowSpace ss(m_block);
			m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
		}
	}

	void Jitmem::getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset/* = 0*/) const
	{
		auto& mem = m_block.dsp().memory();

		switch (_area)
		{
		case MemArea_X:		ptrToReg(_dst, mem.x + offset);	break;
		case MemArea_Y:		ptrToReg(_dst, mem.y + offset);	break;
		case MemArea_P:		ptrToReg(_dst, mem.p + offset);	break;
		default:
			assert(0 && "invalid memory area");
			break;
		}
	}

	void Jitmem::getMemAreaPtr(const JitReg64& _dst, EMemArea _area, const JitReg& _offset) const
	{
		getMemAreaPtr(_dst, _area);

		{
			// use P memory for all bridged external memory
			const auto extMem = m_block.regs().getExtMemAddr();
			const RegGP p(m_block);
			getMemAreaPtr(p.get(), MemArea_P);
			m_block.asm_().cmp(_offset.r32(), extMem.r32());
			m_block.asm_().cmovge(_dst, p);
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

	template asmjit::x86::Mem Jitmem::ptr<uint8_t>(const JitReg64&, const uint8_t*) const;
	template asmjit::x86::Mem Jitmem::ptr<uint32_t>(const JitReg64&, const uint32_t*) const;
	template asmjit::x86::Mem Jitmem::ptr<uint64_t>(const JitReg64&, const uint64_t*) const;

	template void Jitmem::ptrToReg<uint8_t>(const JitReg64&, const uint8_t*) const;
	template void Jitmem::ptrToReg<uint32_t>(const JitReg64&, const uint32_t*) const;
	template void Jitmem::ptrToReg<uint64_t>(const JitReg64&, const uint64_t*) const;
}
