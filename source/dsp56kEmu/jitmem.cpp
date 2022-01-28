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
	
	void Jitmem::mov(uint64_t& _dst, const JitRegGP& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), r64(_src));
	}

	void Jitmem::mov(uint32_t& _dst, const JitRegGP& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().mov(ptr(reg, &_dst), r32(_src));
	}

	void Jitmem::mov(uint32_t& _dst, const JitReg128& _src) const
	{
		const auto reg = regSmallTemp;
		m_block.asm_().movd(ptr(reg, &_dst), _src);
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
		m_block.asm_().movzx(r32(_dst), ptr(r64(_dst), &_src));
#endif
	}

	void Jitmem::makeBasePtr(const JitReg64& _base, const void* _ptr) const
	{
//#ifdef HAVE_ARM64
		m_block.asm_().mov(_base, asmjit::Imm(_ptr));
//#else
		// relocation offset out of range, not useable atm
//		m_block.asm_().lea(_base, asmjit::x86::ptr_rel(reinterpret_cast<uint64_t>(_ptr)));
//#endif
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

	void Jitmem::readDspMemory(const JitRegGP& _dst, const EMemArea _area, const JitRegGP& _offset, const JitReg64& _basePtrPmem/* = JitRegGP()*/) const
	{
		const RegGP t(m_block);
		const SkipLabel skip(m_block.asm_());

#ifdef HAVE_X86_64
		if(asmjit::Support::isPowerOf2(m_block.dsp().memory().size()))
		{
			// just return garbage in case memory is read from an invalid address
			m_block.asm_().and_(_offset, asmjit::Imm(asmjit::Imm(m_block.dsp().memory().size()-1)));
		}
		else
#endif
		{
			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size()));
			m_block.asm_().jge(skip.get());
		}

		getMemAreaPtr(t.get(), _area, _offset, _basePtrPmem);

		m_block.asm_().move(r32(_dst), makePtr(t, _offset, 2, sizeof(TWord)));
	}

	void Jitmem::readDspMemory(const JitRegGP& _dstX, const JitRegGP& _dstY, const JitRegGP& _offsetX, const JitRegGP& _offsetY) const
	{
		const auto pMem = regSmallTemp;
		getMemAreaPtr(pMem, MemArea_P, 0);

		readDspMemory(_dstX, MemArea_X, _offsetX, pMem);
		readDspMemory(_dstY, MemArea_Y, _offsetY, pMem);
	}

	void Jitmem::readDspMemory(const JitRegGP& _dstX, const JitRegGP& _dstY, const JitRegGP& _offset) const
	{
		const RegGP t(m_block);
		const SkipLabel skip(m_block.asm_());

#ifdef HAVE_X86_64
		if (asmjit::Support::isPowerOf2(m_block.dsp().memory().size()))
		{
			// just return garbage in case memory is read from an invalid address
			m_block.asm_().and_(_offset, asmjit::Imm(asmjit::Imm(m_block.dsp().memory().size() - 1)));
		}
		else
#endif
		{
			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size()));
			m_block.asm_().jge(skip.get());
		}

		getMemAreaPtr(regSmallTemp, MemArea_P, 0);

		getMemAreaPtr(t.get(), MemArea_X, _offset, regSmallTemp);
		m_block.asm_().move(r32(_dstX), makePtr(t, _offset, 2, sizeof(TWord)));

		getMemAreaPtr(t.get(), MemArea_Y, _offset, regSmallTemp);
		m_block.asm_().move(r32(_dstY), makePtr(t, _offset, 2, sizeof(TWord)));
	}

	void Jitmem::readDspMemory(const JitRegGP& _dstX, const JitRegGP& _dstY, const TWord& _offset) const
	{
		if (_offset >= m_block.dsp().memory().size())
			return;

		getMemAreaPtr(regSmallTemp, MemArea_X, _offset);
		m_block.asm_().move(r32(_dstX), makePtr(regSmallTemp, 0, sizeof(TWord)));

		const auto& mem = m_block.dsp().memory();
		const auto off = reinterpret_cast<uint64_t>(mem.y) - reinterpret_cast<uint64_t>(mem.x);

		m_block.asm_().add(r64(regSmallTemp), off);
		m_block.asm_().move(r32(_dstY), makePtr(regSmallTemp, 0, sizeof(TWord)));
	}

	void Jitmem::readDspMemory(const JitRegGP& _dst, EMemArea _area, TWord _offset) const
	{
		const auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size() && "memory address out of range");

		if (_offset >= mem.size())
			return;

		const RegGP t(m_block);

		getMemAreaPtr(t.get(), _area, _offset);

		m_block.asm_().move(r32(_dst), makePtr(t, 0, sizeof(uint32_t)));
	}

	void callDSPMemWrite(DSP* const _dsp, const EMemArea _area, const TWord _offset, const TWord _value)
	{
		EMemArea a(_area);
		TWord o(_offset);
		_dsp->memory().dspWrite(a, o, _value);
	}

	void Jitmem::writeDspMemory(const EMemArea _area, const JitRegGP& _offset, const JitRegGP& _src, const JitReg64& _basePtrPmem/* = JitReg64()*/) const
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

		getMemAreaPtr(t.get(), _area, _offset, _basePtrPmem);

		m_block.asm_().mov(makePtr(t, _offset, 2, sizeof(TWord)), r32(_src));
#endif
	}

	void Jitmem::writeDspMemory(const JitRegGP& _offsetX, const JitRegGP& _offsetY, const JitRegGP& _srcX, const JitRegGP& _srcY) const
	{
		const auto pMem = regSmallTemp;
		getMemAreaPtr(pMem, MemArea_P, 0);

		writeDspMemory(MemArea_X, _offsetX, _srcX, pMem);
		writeDspMemory(MemArea_Y, _offsetY, _srcY, pMem);
	}

	void Jitmem::writeDspMemory(const JitRegGP& _offset, const JitRegGP& _srcX, const JitRegGP& _srcY) const
	{
		const RegGP t(m_block);
		const SkipLabel skip(m_block.asm_());

		m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size()));
		m_block.asm_().jge(skip.get());

		getMemAreaPtr(regSmallTemp, MemArea_P, 0);

		getMemAreaPtr(t.get(), MemArea_X, _offset, regSmallTemp);
		m_block.asm_().mov(makePtr(t, _offset, 2, sizeof(TWord)), r32(_srcX));

		getMemAreaPtr(t.get(), MemArea_Y, _offset, regSmallTemp);
		m_block.asm_().mov(makePtr(t, _offset, 2, sizeof(TWord)), r32(_srcY));
	}

	void Jitmem::writeDspMemory(const TWord& _offset, const JitRegGP& _srcX, const JitRegGP& _srcY) const
	{
		if (_offset >= m_block.dsp().memory().size())
			return;

		getMemAreaPtr(regSmallTemp, MemArea_X, _offset);
		m_block.asm_().mov(makePtr(regSmallTemp, 0, sizeof(TWord)), r32(_srcX));

		const auto& mem = m_block.dsp().memory();
		const auto off = reinterpret_cast<uint64_t>(mem.y) - reinterpret_cast<uint64_t>(mem.x);

		m_block.asm_().add(r64(regSmallTemp), off);
		m_block.asm_().mov(makePtr(regSmallTemp, 0, sizeof(TWord)), r32(_srcY));
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

	TWord callDSPMemReadPeriph(DSP* const _dsp, const TWord _area, const TWord _offset, Instruction _inst)
	{
		return _dsp->getPeriph(_area)->read(_offset, _inst);
	}

	void callDSPMemWritePeriph(DSP* const _dsp, const TWord _area, const TWord _offset, const TWord _value)
	{
		_dsp->getPeriph(_area)->write(_offset, _value);
	}

	void Jitmem::readPeriph(const JitReg64& _dst, EMemArea _area, const TWord& _offset, Instruction _inst) const
	{
		FuncArg r0(m_block, 0);
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(r0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(r2, asmjit::Imm(_offset));
		m_block.asm_().mov(r3, asmjit::Imm(_inst));

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::readPeriph(const JitReg64& _dst, const EMemArea _area, const JitReg64& _offset, Instruction _inst) const
	{
		FuncArg r0(m_block, 0);
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(r0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(r2, _offset);
		m_block.asm_().mov(r3, asmjit::Imm(_inst));

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));

		m_block.asm_().mov(_dst, regReturnVal);
	}

	void Jitmem::writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const
	{
		FuncArg r0(m_block, 0);
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(r0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(r2, _offset);
		m_block.asm_().mov(r3, _value);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
	}

	void Jitmem::writePeriph(EMemArea _area, const TWord& _offset, const JitReg64& _value) const
	{
		FuncArg r0(m_block, 0);
		FuncArg r1(m_block, 1);
		FuncArg r2(m_block, 2);
		FuncArg r3(m_block, 3);

		m_block.asm_().mov(r0, asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r1, _area == MemArea_Y ? 1 : 0);
		m_block.asm_().mov(r2, asmjit::Imm(_offset));
		m_block.asm_().mov(r3, _value);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
	}

	void Jitmem::getPMemBasePtr(const JitReg64& _dst) const
	{
		getMemAreaPtr(_dst, MemArea_P);
	}

	void Jitmem::getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset/* = 0*/, const JitRegGP& _ptrToPmem/* = JitRegGP()*/) const
	{
		auto& mem = m_block.dsp().memory();

		TWord* ptr = nullptr;

		switch (_area)
		{
		case MemArea_X:		ptr = mem.x; break;
		case MemArea_Y:		ptr = mem.y; break;
		case MemArea_P:		ptr = mem.p; break;
		default:
			assert(0 && "invalid memory area");
			break;
		}

		if(_ptrToPmem.isValid())
		{
			const auto off = reinterpret_cast<int64_t>(ptr) - reinterpret_cast<int64_t>(mem.p) + static_cast<int64_t>(offset) * sizeof(TWord);
#ifdef HAVE_ARM64
			m_block.asm_().add(_dst, _ptrToPmem, off);
#else
			m_block.asm_().lea(_dst, asmjit::x86::ptr(_ptrToPmem, static_cast<int>(off)));
#endif
		}
		else
		{
			return ptrToReg(_dst, ptr + offset);
		}
	}

	void Jitmem::getMemAreaPtr(const JitReg64& _dst, EMemArea _area, const JitRegGP& _offset, const JitReg64& _ptrToPmem/* = JitRegGP()*/) const
	{
		// as we bridge to P memory there is no need to do anything here if area is P already
		if (_area == MemArea_P)
		{
			getMemAreaPtr(_dst, _area);
			return;
		}

		// use P memory for all bridged external memory
		auto p = _ptrToPmem;
		if(!p.isValid())
		{
			p = regSmallTemp;
			getMemAreaPtr(p, MemArea_P);
		}
		getMemAreaPtr(_dst, _area, 0, p);

		m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().getBridgedMemoryAddress()));
#ifdef HAVE_ARM64
		m_block.asm_().csel(_dst, p, _dst, asmjit::arm::CondCode::kGE);
#else
		m_block.asm_().cmovge(_dst, p);
#endif
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
			makeBasePtr(_r, _t);
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
