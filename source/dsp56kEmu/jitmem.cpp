#include "jitmem.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitemitter.h"
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

	void Jitmem::mov(const JitRegGP& _dst, const TReg24& _src)
	{
		const JitReg32 foo = r32(_dst);
		const JitMemPtr bar = ptr(r64(_dst), _src);
		m_block.asm_().move(foo, bar);
	}

	void Jitmem::mov(const JitRegGP& _dst, const TReg48& _src)
	{
		m_block.asm_().move(r64(_dst), ptr(r64(_dst), _src));
	}

	void Jitmem::mov(const JitRegGP& _dst, const TReg56& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().move(r64(_dst), ptr(reg, _src));
	}

	void Jitmem::mov(const TReg24& _dst, const JitRegGP& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, _dst), r32(_src));
	}

	void Jitmem::mov(const TReg48& _dst, const JitRegGP& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, _dst), r64(_src));
	}

	void Jitmem::mov(const TReg56& _dst, const JitRegGP& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, _dst), r64(_src));
	}

	void Jitmem::mov(const JitReg128& _dst, TReg56& _src)
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movq(_dst, ptr(reg, _src));
	}

	void Jitmem::mov(uint64_t& _dst, const JitRegGP& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), _src);
	}

	void Jitmem::mov(uint32_t& _dst, const JitRegGP& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), r32(_src));
	}

	void Jitmem::mov(uint8_t& _dst, const JitRegGP& _src) const
	{
		const auto reg = regSmallTemp;
#ifdef HAVE_ARM64
		m_block.asm_().strb(r32(_src), ptr(reg, &_dst));
#else
		m_block.asm_().mov(ptr(reg, &_dst), _src.r8());
#endif
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint64_t& _src) const
	{
		m_block.asm_().move(_dst, ptr(r64(_dst), &_src));
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint32_t& _src) const
	{
		m_block.asm_().move(r32(_dst), ptr(r64(_dst), &_src));
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint8_t& _src) const
	{
#ifdef HAVE_ARM64
		m_block.asm_().ldrb(r32(_dst), ptr(r64(_dst), &_src));
#else
		m_block.asm_().movzx(_dst.r8(), ptr(r64(_dst), &_src));
#endif
	}

	void Jitmem::mov(void* _dst, void* _src, uint32_t _size)
	{
		const RegGP v(m_block);
		const auto a = regReturnVal;

		m_block.asm_().mov(a, asmjit::Imm(_src));
		m_block.asm_().move(v, makePtr(a, 0, _size));
		m_block.asm_().mov(a, asmjit::Imm(_dst));
		m_block.asm_().mov(makePtr(a, 0, _size), v.get());
	}

	void Jitmem::mov(void* _dst, const JitRegGP& _src, uint32_t _size)
	{
		const auto a = regReturnVal;
		m_block.asm_().mov(a, asmjit::Imm(_dst));
		m_block.asm_().mov(makePtr(a, 0, _size), _src);
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _base, const JitRegGP& _index, const uint32_t _shift, const uint32_t _size)
	{
#ifdef HAVE_ARM64
		return asmjit::arm::ptr(_base, _index, asmjit::arm::Shift(asmjit::arm::ShiftOp::kLSL, _shift));
#else
		return asmjit::x86::ptr(_base, _index, _shift, 0, _size);
#endif
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _base, const uint32_t _offset, const uint32_t _size)
	{
#ifdef HAVE_ARM64
		return asmjit::arm::ptr(_base, _offset);
#else
		return asmjit::x86::ptr(_base, _offset, _size);
#endif
	}

	void Jitmem::setPtrOffset(JitMemPtr& _mem, const void* _base, const void* _member)
	{
		_mem.setOffset(reinterpret_cast<uint64_t>(_member) - reinterpret_cast<uint64_t>(_base));
	}

	void Jitmem::readDspMemory(const JitRegGP& _dst, const EMemArea _area, const JitRegGP& _offset) const
	{
		const RegGP t(m_block);
		const SkipLabel skip(m_block.asm_());

		m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size()));
		m_block.asm_().jge(skip.get());

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().move(r32(_dst), makePtr(t, _offset, 2, sizeof(TWord)));
	}
	
	void callDSPMemWrite(DSP* const _dsp, const EMemArea _area, const TWord _offset, const TWord _value)
	{
		EMemArea a(_area);
		TWord o(_offset);
		_dsp->memory().dspWrite(a, o, _value);
	}

	void Jitmem::writeDspMemory(const EMemArea _area, const JitRegGP& _offset, const JitRegGP& _src) const
	{
#ifdef DEBUG_MEMORY_WRITES
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(g_funcArgGPs[0], asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r1, _area);
		m_block.asm_().mov(r2, _offset);
		m_block.asm_().mov(r3, _src);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWrite));
#else
		const RegGP t(m_block);

		const SkipLabel skip(m_block.asm_());

		m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size()));
		m_block.asm_().jge(skip.get());

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().mov(makePtr(t, _offset, 2, sizeof(TWord)), r32(_src));
#endif
	}

	void Jitmem::readDspMemory(const JitRegGP& _dst, EMemArea _area, TWord _offset) const
	{
		auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size() && "memory address out of range");

		if(_offset >= mem.size())
			return;

		const RegGP t(m_block);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().move(r32(_dst), makePtr(t, 0, sizeof(uint32_t)));
	}

	void Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const JitRegGP& _src) const
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

		m_block.asm_().mov(makePtr(t, 0, sizeof(uint32_t)), r32(_src));
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
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);

		m_block.asm_().mov(g_funcArgGPs[0], asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(g_funcArgGPs[1], _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(g_funcArgGPs[2], asmjit::Imm(_offset));

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::readPeriph(const JitReg64& _dst, const EMemArea _area, const JitReg64& _offset) const
	{
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);

		m_block.asm_().mov(g_funcArgGPs[0], asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(g_funcArgGPs[1], _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(g_funcArgGPs[2], _offset);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const
	{
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(g_funcArgGPs[0], asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(g_funcArgGPs[1], _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(g_funcArgGPs[2], _offset);
		m_block.asm_().mov(g_funcArgGPs[3], _value);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
	}

	void Jitmem::writePeriph(EMemArea _area, const TWord& _offset, const JitReg64& _value) const
	{
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(g_funcArgGPs[0], asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(g_funcArgGPs[1], _area == MemArea_Y ? 1 : 0);

		m_block.asm_().mov(g_funcArgGPs[2], asmjit::Imm(_offset));
		m_block.asm_().mov(g_funcArgGPs[3], _value);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
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

	void Jitmem::getMemAreaPtr(const JitReg64& _dst, EMemArea _area, const JitRegGP& _offset) const
	{
		getMemAreaPtr(_dst, _area);

		{
			// use P memory for all bridged external memory
			const auto p = regSmallTemp;
			getMemAreaPtr(p, MemArea_P);
			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().getBridgedMemoryAddress()));
#ifdef HAVE_ARM64
			m_block.asm_().csel(_dst, p, _dst, asmjit::arm::CondCode::kGE);
#else
			m_block.asm_().cmovge(_dst, p);
#endif
		}
	}

	template<typename T>
	JitMemPtr Jitmem::ptr(const JitReg64& _temp, const T* _t) const
	{
		ptrToReg<T>(_temp, _t);
		return makePtr(_temp, 0, sizeof(T));
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

	template JitMemPtr Jitmem::ptr<uint8_t>(const JitReg64&, const uint8_t*) const;
	template JitMemPtr Jitmem::ptr<uint32_t>(const JitReg64&, const uint32_t*) const;
	template JitMemPtr Jitmem::ptr<uint64_t>(const JitReg64&, const uint64_t*) const;

	template void Jitmem::ptrToReg<uint8_t>(const JitReg64&, const uint8_t*) const;
	template void Jitmem::ptrToReg<uint32_t>(const JitReg64&, const uint32_t*) const;
	template void Jitmem::ptrToReg<uint64_t>(const JitReg64&, const uint64_t*) const;
}
