#include "jitmem.h"

#include "dsp.h"
#include "jitblock.h"
#include "jithelper.h"
#include "jitregtracker.h"

//#define DEBUG_MEMORY_WRITES

namespace dsp56k
{
	const auto regSmallTemp = regReturnVal;
	
	void Jitmem::mov(TReg24& _dst, const JitReg128& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movd(ptr(reg, _dst), _src);
	}

	void Jitmem::mov(TReg48& _dst, const JitReg128& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movq(ptr(reg, _dst), _src);
	}

	void Jitmem::mov(TReg56& _dst, const JitReg128& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movq(ptr(reg, _dst), _src);
	}

	void Jitmem::mov(const JitReg128& _dst, TReg24& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movd(_dst, ptr(reg, _src));
	}

	void Jitmem::mov(const JitReg128& _dst, TReg48& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movq(_dst, ptr(reg, _src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const TReg24& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(_dst.r32(), ptr(reg, _src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const TReg48& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(_dst.r64(), ptr(reg, _src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const TReg56& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(_dst.r64(), ptr(reg, _src));
	}

	void Jitmem::mov(const TReg24& _dst, const asmjit::x86::Gp& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, _dst), _src.r32());
	}

	void Jitmem::mov(const TReg48& _dst, const asmjit::x86::Gp& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, _dst), _src.r64());
	}

	void Jitmem::mov(const TReg56& _dst, const asmjit::x86::Gp& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, _dst), _src.r64());
	}

	void Jitmem::mov(const JitReg128& _dst, TReg56& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movq(_dst, ptr(reg, _src));
	}

	void Jitmem::mov(uint64_t& _dst, const asmjit::x86::Gp& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(uint32_t& _dst, const asmjit::x86::Gp& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(uint8_t& _dst, const asmjit::x86::Gp& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const uint64_t& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const uint32_t& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(const asmjit::x86::Gp& _dst, const uint8_t& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movzx(_dst, ptr(reg, &_src));
	}

	void Jitmem::mov(void* _dst, void* _src, uint32_t _size)
	{
		const RegGP a(m_block);
		const RegGP v(m_block);

		m_block.asm_().mov(a, asmjit::Imm(_src));
		m_block.asm_().mov(v, asmjit::x86::ptr(a, 0, _size));
		m_block.asm_().mov(a, asmjit::Imm(_dst));
		m_block.asm_().mov(asmjit::x86::ptr(a, 0, _size), v.get());
	}

	void Jitmem::mov(void* _dst, const JitReg& _src, uint32_t _size)
	{
		const RegGP a(m_block);
		m_block.asm_().mov(a, asmjit::Imm(_dst));
		m_block.asm_().mov(asmjit::x86::ptr(a, 0, _size), _src);
	}

	void Jitmem::readDspMemory(const JitReg& _dst, const EMemArea _area, const JitReg& _offset) const
	{
		const SkipLabel skip(m_block.asm_());

		m_block.asm_().cmp(_offset.r32(), asmjit::Imm(m_block.dsp().memory().size()));
		m_block.asm_().jge(skip.get());
		
		const RegGP t(m_block);
		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(_dst.r32(), asmjit::x86::ptr(t, _offset, 2, 0, sizeof(TWord)));
	}
	
	void callDSPMemWrite(DSP* const _dsp, const EMemArea _area, const TWord _offset, const TWord _value)
	{
		EMemArea a(_area);
		TWord o(_offset);
		_dsp->memory().dspWrite(a, o, _value);
	}

	void Jitmem::writeDspMemory(const EMemArea _area, const JitReg& _offset, const JitReg& _src) const
	{
#ifdef DEBUG_MEMORY_WRITES
		PushGP r8(m_block, asmjit::x86::r8);
		PushGP r9(m_block, asmjit::x86::r9);
		PushGP r10(m_block, asmjit::x86::r10);
		PushGP r11(m_block, asmjit::x86::r11);
		PushGP rbp(m_block, asmjit::x86::rbp);
		PushGP padding(m_block, asmjit::x86::rbp);
		
		m_block.asm_().mov(regArg0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(regArg1, _area);

		PushGP r2(m_block, regArg2);
		PushGP r3(m_block, regArg3);
		PushGP rPadding(m_block, regArg3);

		m_block.asm_().mov(regArg2, _offset);
		m_block.asm_().mov(regArg3, _src);

		{
			PushXMMRegs xmms(m_block);
			PushShadowSpace ss(m_block);
			m_block.asm_().call(asmjit::func_as_ptr(&callDSPMemWrite));
		}
#else
		const SkipLabel skip(m_block.asm_());

		m_block.asm_().cmp(_offset.r32(), asmjit::Imm(m_block.dsp().memory().size()));
		m_block.asm_().jge(skip.get());

		const RegGP t(m_block);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(asmjit::x86::ptr(t, _offset, 2, 0, sizeof(TWord)), _src.r32());
#endif
	}

	void Jitmem::readDspMemory(const JitReg& _dst, EMemArea _area, TWord _offset) const
	{
		auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size() && "memory address out of range");

		if(_offset >= mem.size())
			return;

		const RegGP t(m_block);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(_dst.r32(), asmjit::x86::ptr(t));
	}

	void Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const JitReg& _src) const
	{
#ifdef DEBUG_MEMORY_WRITES
		RegGP r(m_block);
		m_block.asm_().mov(r, asmjit::Imm(_offset));
		writeDspMemory(_area, r.get(), _src);
#else
		auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size() && "memory address out of range");

		if(_offset >= mem.size())
			return;

		const RegGP t(m_block);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(asmjit::x86::ptr(t), _src.r32());
#endif
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
			PushBeforeFunctionCall backup(m_block);
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
			PushBeforeFunctionCall backup(m_block);
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
			PushBeforeFunctionCall backup(m_block);
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
			PushBeforeFunctionCall backup(m_block);
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
			const auto p = regSmallTemp;
			getMemAreaPtr(p, MemArea_P);
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
