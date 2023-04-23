#include "jitmem.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitdspvalue.h"
#include "jitemitter.h"
#include "jithelper.h"
#include "jitregtracker.h"

constexpr bool g_debugMemoryWrites = false;

namespace dsp56k
{
	void Jitmem::mov(void* _dst, const uint32_t& _imm) const
	{
		const RegGP src(m_block);
		m_block.asm_().mov(r32(src), asmjit::Imm(_imm));
		mov(_dst, src, sizeof(uint32_t));
	}

	void Jitmem::mov(void* _dst, const uint64_t& _imm) const
	{
		const RegGP src(m_block);
		m_block.asm_().mov(r64(src), asmjit::Imm(_imm));
		mov(_dst, src, sizeof(uint64_t));
	}

	void Jitmem::mov(void* _dst, const JitRegGP& _src, const uint32_t _size) const
	{
		const RegScratch reg(m_block);
		const auto p = makePtr(reg, _dst, _size);
		mov(p, _src);
	}

	void Jitmem::mov(uint64_t& _dst, const JitRegGP& _src) const
	{
		mov(&_dst, _src, sizeof(uint64_t));
	}

	void Jitmem::mov(uint64_t& _dst, const DspValue& _src) const
	{
		assert(_src.getBitCount() > 32);

		if(_src.isImmediate())
		{
			mov(&_dst, reinterpret_cast<const uint64_t&>(_src.imm()));
		}
		else
		{
			mov(&_dst, _src.get(), sizeof(uint64_t));
		}
	}

	void Jitmem::mov(uint32_t& _dst, const DspValue& _src) const
	{
		assert(_src.getBitCount() <= 32);

		if(_src.isImmediate())
		{
			mov(&_dst, _src.imm24());
		}
		else
		{
			mov(&_dst, _src.get(), sizeof(uint32_t));
		}
	}

	void Jitmem::mov(uint32_t& _dst, const JitReg128& _src) const
	{
		const RegScratch reg(m_block);
		m_block.asm_().movd(makePtr(reg, &_dst, sizeof(uint32_t)), _src);
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint64_t& _src) const
	{
		mov(_dst, makePtr(r64(_dst), &_src, sizeof(_src)));
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint32_t& _src) const
	{
		mov(_dst, makePtr(r64(_dst), &_src, sizeof(_src)));
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint8_t& _src) const
	{
		mov(_dst, makePtr(r64(_dst), &_src, sizeof(_src)));
	}

	void Jitmem::mov(const JitMemPtr& _dst, const JitRegGP& _src) const
	{
		switch (_dst.size())
		{
		case sizeof(TWord):
			m_block.asm_().mov(_dst, r32(_src));
			break;
		case sizeof(uint64_t):
			m_block.asm_().mov(_dst, r64(_src));
			break;
		case sizeof(uint8_t):
#ifdef HAVE_ARM64
			m_block.asm_().strb(r32(_src), _dst);
#else
			m_block.asm_().mov(_dst, _src.r8());
#endif
			break;
		default:
			assert(false && "unknown memory size");
			break;
		}
	}

	void Jitmem::mov(const JitRegGP& _dst, const JitMemPtr& _src) const
	{
		switch (_src.size())
		{
		case sizeof(TWord):
			m_block.asm_().mov(r32(_dst), _src);
			break;
		case sizeof(uint64_t):
			m_block.asm_().mov(r64(_dst), _src);
			break;
		case sizeof(uint8_t):
#ifdef HAVE_ARM64
			m_block.asm_().ldrb(r32(_dst), _src);
#else
			m_block.asm_().movzx(r32(_dst), _src);
#endif
			break;
		default:
			assert(false && "unknown memory size");
			break;
		}
	}

	void Jitmem::mov(const JitMemPtr& _dst, const uint64_t& _immSrc) const
	{
		const auto imm = asmjit::Imm(_immSrc);

		assert(_dst.size());

		JitRegGP temp;
		RegGP t(m_block, false);

		RegScratch ppmem(m_block);
#ifdef HAVE_ARM64
		if(!_dst.hasBaseReg() || asmjit::arm::Reg::fromTypeAndId(_dst.baseType(), _dst.baseId()) != ppmem)
#else
		if (_dst.baseReg() != ppmem.get())
#endif
		{
			temp = ppmem.get();
		}
		else
		{
			t.acquire();
			temp = t.get();
		}

#ifdef HAVE_ARM64
		if (_dst.size() <= sizeof(uint32_t))
			m_block.asm_().mov(r32(temp), imm);
		else
			m_block.asm_().mov(r64(temp), imm);
		mov(_dst, temp);
#else
		m_block.asm_().mov(_dst, imm);
#endif
	}

	void Jitmem::makeBasePtr(const JitReg64& _base, const void* _ptr, const size_t _size/* = sizeof(uint64_t)*/) const
	{
#ifdef HAVE_ARM64
		m_block.asm_().mov(_base, asmjit::Imm(_ptr));
#else
		const auto p = m_block.dspRegPool().makeDspPtr(_ptr, _size);
		if(p.hasSize())
			m_block.asm_().lea(_base, p);
		else
			m_block.asm_().mov(_base, asmjit::Imm(reinterpret_cast<uint64_t>(_ptr)));
#endif
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _base, const JitRegGP& _index, const uint32_t _shift, const uint32_t _size)
	{
#ifdef HAVE_ARM64
		auto p = asmjit::arm::ptr(_base, _index, asmjit::arm::Shift(asmjit::arm::ShiftOp::kLSL, _shift));
		p.setSize(_size);
		return p;
#else
		return asmjit::x86::ptr(_base, _index, _shift, 0, _size);
#endif
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _base, const uint32_t _size)
	{
#ifdef HAVE_ARM64
		auto p = asmjit::arm::ptr(_base, 0);
		p.setSize(_size);
		return p;
#else
		return asmjit::x86::ptr(_base, 0, _size);
#endif
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _scratch, const void* _hostPtr, const uint32_t _size) const
	{
		const auto p = m_block.dspRegPool().makeDspPtr(_hostPtr, _size);

		if(p.hasSize())
			return p;

		makeBasePtr(_scratch, _hostPtr, _size);
		return makePtr(_scratch, _size);
	}

	JitMemPtr Jitmem::makeRelativePtr(const void* _ptr, const void* _base, const JitReg64& _baseReg, const size_t _size)
	{
		const auto offset = static_cast<const uint8_t*>(_ptr) - static_cast<const uint8_t*>(_base);
#ifdef HAVE_ARM64
		bool canBeEncoded = false;

		if(offset >= -256 && offset <= 255)
		{
			canBeEncoded = true;
		}
		else if(offset >= 0)
		{
			if( (_size == sizeof(uint32_t) && offset <= 16380 && (offset & 3) == 0) ||
				(_size == sizeof(uint64_t) && offset <= 32760 && (offset & 7) == 0))
				canBeEncoded = true;
		}
#else
		const auto canBeEncoded = offset >= std::numeric_limits<int>::min() && offset <= std::numeric_limits<int>::max();
#endif
		if(!canBeEncoded)
			return makePtr(_baseReg, 0);

		auto p = makePtr(_baseReg, static_cast<uint32_t>(_size));
		p.setOffset(offset);

		return p;
	}

	void Jitmem::readDspMemory(DspValue& _dst, const EMemArea _area, const JitRegGP& _offset, ScratchPMem& _basePtrPmem) const
	{
		ScratchPMem t(m_block);
		t.temp(DspValue::Memory);

		// push onto stack (if applicable) before branch
		if(!_basePtrPmem.isRegValid())
		{
			_basePtrPmem.temp(DspValue::Memory);
			_basePtrPmem.release();
		}

		const SkipLabel skip(m_block.asm_());

		if (!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

#ifdef HAVE_X86_64
		if(asmjit::Support::isPowerOf2(m_block.dsp().memory().size(_area)))
		{
			// just return garbage in case memory is read from an invalid address
			m_block.asm_().and_(r32(_offset), asmjit::Imm(asmjit::Imm(m_block.dsp().memory().size(_area)-1)));
		}
		else
#endif
		{
			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size(_area)));
			m_block.asm_().jge(skip.get());
		}

		const auto p = getMemAreaPtr(t, _area, _offset, _basePtrPmem);

		readDspMemory(_dst, p);
	}

	void Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offsetX, const JitRegGP& _offsetY) const
	{
		ScratchPMem pMem(m_block, false, true);
		readDspMemory(_dstX, MemArea_X, _offsetX, pMem);
		readDspMemory(_dstY, MemArea_Y, _offsetY, pMem);
	}

	void Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offset) const
	{
		ScratchPMem t(m_block);
		t.temp(DspValue::Memory);

		if (!_dstX.isRegValid())
			_dstX.temp(DspValue::Memory);
		if (!_dstY.isRegValid())
			_dstY.temp(DspValue::Memory);

		const SkipLabel skip(m_block.asm_());

#ifdef HAVE_X86_64
		if (asmjit::Support::isPowerOf2(m_block.dsp().memory().sizeXY()))
		{
			// just return garbage in case memory is read from an invalid address
			m_block.asm_().and_(_offset, asmjit::Imm(asmjit::Imm(m_block.dsp().memory().sizeXY() - 1)));
		}
		else
#endif
		{
			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().sizeXY()));
			m_block.asm_().jge(skip.get());
		}

		ScratchPMem pmem(m_block, false, true);

		auto p = getMemAreaPtr(t, MemArea_X, _offset, pmem);
		readDspMemory(_dstX, p);

		p = getMemAreaPtr(t, MemArea_Y, _offset, pmem);
		readDspMemory(_dstY, p);
	}

	void Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const TWord& _offset) const
	{
		if (_offset >= m_block.dsp().memory().sizeXY())
			return;

		if (!_dstX.isRegValid())
			_dstX.temp(DspValue::Memory);
		if (!_dstY.isRegValid())
			_dstY.temp(DspValue::Memory);

		ScratchPMem pxmem(m_block, false, true);
		auto p = getMemAreaPtr(pxmem, MemArea_X, _offset);
		readDspMemory(_dstX, p);

		const auto& mem = m_block.dsp().memory();

		if(_offset < mem.getBridgedMemoryAddress())
		{
			const auto off = reinterpret_cast<uint64_t>(mem.y) - reinterpret_cast<uint64_t>(mem.x);

#ifdef HAVE_ARM64
			if (pxmem.isRegValid() && off < 0x1000000) // ARM can only deal with 24 bit immediates in an add()
#else
			if (pxmem.isRegValid())
#endif
			{
				m_block.asm_().add(r64(pxmem), off);
			}
			else
				p = getMemAreaPtr(pxmem, MemArea_Y, _offset);
		}
		else
		{
			assert(false);
		}

		readDspMemory(_dstY, p);
	}

	void Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset) const
	{
		ScratchPMem pmem(m_block, false, true);
		readDspMemory(_dst, _area, _offset, pmem);
	}

	void Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset, ScratchPMem& _basePtrPmem) const
	{
		const auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size(_area) && "memory address out of range");

		if (_offset >= mem.size(_area))
			return;

		ScratchPMem t(m_block);

		const auto p = getMemAreaPtr(t, _area, _offset, _basePtrPmem);

		if (!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

		readDspMemory(_dst, p);
	}

	void Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, const DspValue& _offset) const
	{
		ScratchPMem pmem(m_block, false, true);
		readDspMemory(_dst, _area, _offset, pmem);
	}

	void Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, const DspValue& _offset, ScratchPMem& _basePtrPmem) const
	{
		if (_offset.isType(DspValue::Immediate24))
			readDspMemory(_dst, _area, _offset.imm24(), _basePtrPmem);
		else
			readDspMemory(_dst, _area, _offset.get(), _basePtrPmem);
	}

	void Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offsetX, const DspValue& _offsetY) const
	{
		if(_offsetX.isImm24() || _offsetY.isImm24())
		{
			ScratchPMem ppmem(m_block, false, true);

			if (_offsetX.isImm24())
				readDspMemory(_dstX, MemArea_X, _offsetX.imm24(), ppmem);
			else
				readDspMemory(_dstX, MemArea_X, _offsetX.get(), ppmem);
			if (_offsetY.isImm24())
				readDspMemory(_dstY, MemArea_Y, _offsetY.imm24(), ppmem);
			else
				readDspMemory(_dstY, MemArea_Y, _offsetY.get(), ppmem);
		}
		else
		{
			readDspMemory(_dstX, _dstY, _offsetX.get(), _offsetY.get());
		}
	}

	void Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offset) const
	{
		if (_offset.isImm24())
			readDspMemory(_dstX, _dstY, _offset.imm24());
		else
			readDspMemory(_dstX, _dstY, _offset.get());
	}

	void callDSPMemWrite(DSP* const _dsp, const EMemArea _area, const TWord _offset, const TWord _value)
	{
		EMemArea a(_area);
		TWord o(_offset);
		_dsp->memory().dspWrite(a, o, _value);
	}

	void Jitmem::writeDspMemory(const EMemArea _area, const JitRegGP& _offset, const DspValue& _src, ScratchPMem& _basePtrPmem) const
	{
		if(m_block.getConfig().memoryWritesCallCpp || g_debugMemoryWrites)
		{
			const FuncArg r0(m_block, 0);
			const FuncArg r1(m_block, 1);
			const FuncArg r2(m_block, 2);
			const FuncArg r3(m_block, 3);

			if(_src.isImm24())
			{
				m_block.asm_().mov(r32(r2), r32(_offset));
				m_block.asm_().mov(r64(r0), asmjit::Imm(&m_block.dsp()));
				m_block.asm_().mov(r32(r1), asmjit::Imm(_area));
				m_block.asm_().mov(r32(r3), asmjit::Imm(_src.imm24()));
			}
			else
			{
				if(m_block.stack().isUsedFuncArg(_offset) && m_block.stack().isUsedFuncArg(_src.get()))
				{
					const RegScratch t(m_block);
					m_block.asm_().mov(r32(t), r32(_src.get()));
					m_block.asm_().mov(r32(r2), r32(_offset));
					m_block.asm_().mov(r32(r3), r32(t));
				}
				else if(m_block.stack().isUsedFuncArg(_src.get()))
				{
					m_block.asm_().mov(r32(r3), r32(_src.get()));
					m_block.asm_().mov(r32(r2), r32(_offset));
				}
				else
				{
					m_block.asm_().mov(r32(r2), r32(_offset));
					m_block.asm_().mov(r32(r3), r32(_src.get()));
				}

				m_block.asm_().mov(r64(r0), asmjit::Imm(&m_block.dsp()));
				m_block.asm_().mov(r32(r1), asmjit::Imm(_area));
			}

			m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWrite));
		}
		else
		{
			ScratchPMem t(m_block);
			t.temp(DspValue::Memory);

			// push onto stack (if applicable) before branch
			if(!_basePtrPmem.isRegValid())
			{
				_basePtrPmem.temp(DspValue::Memory);
				_basePtrPmem.release();
			}

			const SkipLabel skip(m_block.asm_());

			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size(_area)));
			m_block.asm_().jge(skip.get());

			const auto p = getMemAreaPtr(t, _area, _offset, _basePtrPmem);

			writeDspMemory(p, _src);
		}
	}

	void Jitmem::writeDspMemory(EMemArea _area, const JitRegGP& _offset, const DspValue& _src) const
	{
		ScratchPMem pmem(m_block, false, true);
		writeDspMemory(_area, _offset, _src, pmem);
	}

	void Jitmem::writeDspMemory(const JitRegGP& _offsetX, const JitRegGP& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const
	{
		ScratchPMem pMem(m_block, false, true);
		writeDspMemory(MemArea_X, _offsetX, _srcX, pMem);
		writeDspMemory(MemArea_Y, _offsetY, _srcY, pMem);
	}

	void Jitmem::writeDspMemory(const JitRegGP& _offset, const DspValue& _srcX, const DspValue& _srcY) const
	{
		ScratchPMem t(m_block);
		t.temp(DspValue::Memory);

		const SkipLabel skip(m_block.asm_());

		m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().sizeXY()));
		m_block.asm_().jge(skip.get());

		ScratchPMem pMem(m_block, false, true);

		auto p = getMemAreaPtr(t, MemArea_X, _offset, pMem);
		writeDspMemory(p, _srcX);

		p = getMemAreaPtr(t, MemArea_Y, _offset, pMem);
		writeDspMemory(p, _srcY);
	}

	void Jitmem::writeDspMemory(const TWord& _offset, const DspValue& _srcX, const DspValue& _srcY) const
	{
		if (_offset >= m_block.dsp().memory().sizeXY())
			return;

		ScratchPMem pxmem(m_block, false, true);
		auto p = getMemAreaPtr(pxmem, MemArea_X, _offset);
		writeDspMemory(p, _srcX);

		const auto& mem = m_block.dsp().memory();

		if(_offset < mem.getBridgedMemoryAddress())
		{
			const auto off = reinterpret_cast<uint64_t>(mem.y) - reinterpret_cast<uint64_t>(mem.x);

#ifdef HAVE_ARM64
			if (pxmem.isRegValid() && off < 0x1000000) // ARM can only deal with 24 bit immediates in an add()
#else
			if (pxmem.isRegValid())
#endif
			{
				m_block.asm_().add(r64(pxmem), off);
			}
			else
				p = getMemAreaPtr(pxmem, MemArea_Y, _offset);
		}
		else
		{
			assert(false);
		}
		writeDspMemory(p, _srcY);
	}

	void Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src) const
	{
		ScratchPMem pmem(m_block, false, true);
		writeDspMemory(_area, _offset, _src, pmem);
	}

	void Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src, ScratchPMem& _basePtrPmem) const
	{
		if(m_block.getConfig().memoryWritesCallCpp || g_debugMemoryWrites)
		{
			const RegGP  r(m_block);
			m_block.asm_().mov(r, asmjit::Imm(_offset));
			writeDspMemory(_area, r.get(), _src);
		}
		else
		{
			const auto& mem = m_block.dsp().memory();
			mem.memTranslateAddress(_area, _offset);

			assert(_offset < mem.size(_area) && "memory address out of range");

			if(_offset >= mem.size(_area))
				return;

			ScratchPMem t(m_block);

			const auto p = getMemAreaPtr(t, _area, _offset, _basePtrPmem);

			writeDspMemory(p, _src);
		}
	}

	void Jitmem::writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src) const
	{
		ScratchPMem pmem(m_block, false, true);
		writeDspMemory(_area, _offset, _src, pmem);
	}

	void Jitmem::writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src, ScratchPMem& _basePtrPmem) const
	{
		if (_offset.isType(DspValue::Immediate24))
			writeDspMemory(_area, _offset.imm24(), _src, _basePtrPmem);
		else
			writeDspMemory(_area, _offset.get(), _src, _basePtrPmem);
	}

	void Jitmem::writeDspMemory(const DspValue& _offsetX, const DspValue& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const
	{
		if(_offsetX.isImm24() || _offsetY.isImm24())
		{
			ScratchPMem ppmem(m_block, false, true);
			if(_offsetX.isImm24())
				writeDspMemory(MemArea_X, _offsetX.imm24(), _srcX, ppmem);
			else
				writeDspMemory(MemArea_X, _offsetX.get(), _srcX, ppmem);

			if (_offsetY.isImm24())
				writeDspMemory(MemArea_Y, _offsetY.imm24(), _srcY, ppmem);
			else
				writeDspMemory(MemArea_Y, _offsetY.get(), _srcY, ppmem);
		}
		else
		{
			writeDspMemory(_offsetX.get(), _offsetY.get(), _srcX, _srcY);
		}
	}

	void Jitmem::writeDspMemory(const DspValue& _offset, const DspValue& _srcX, const DspValue& _srcY) const
	{
		if (_offset.isImm24())
			writeDspMemory(_offset.imm24(), _srcX, _srcY);
		else
			writeDspMemory(_offset.get(), _srcX, _srcY);
	}

	TWord callDSPMemReadPeriph(DSP* const _dsp, const TWord _area, const TWord _offset, Instruction _inst)
	{
		return _dsp->getPeriph(_area)->read(_offset, _inst);
	}

	void callDSPMemWritePeriph(DSP* const _dsp, const TWord _area, const TWord _offset, const TWord _value)
	{
		_dsp->getPeriph(_area)->write(_offset, _value);
	}

	void Jitmem::readPeriph(DspValue& _dst, const EMemArea _area, const TWord& _offset, const Instruction _inst) const
	{
		auto* periph = m_block.dsp().getPeriph(_area);

		const auto* memPtr = periph->readAsPtr(_offset, _inst);

		if(memPtr)
		{
			if (!_dst.isRegValid())
				_dst.temp(DspValue::Memory);

			mov(_dst.get(), *memPtr);
			return;
		}

		{
			const FuncArg r0(m_block, 0);
			const FuncArg r1(m_block, 1);
			const FuncArg r2(m_block, 2);
			const FuncArg r3(m_block, 3);

			m_block.asm_().mov(r64(r0), asmjit::Imm(&m_block.dsp()));
			m_block.asm_().mov(r32(r1), asmjit::Imm(_area == MemArea_Y ? 1 : 0));
			m_block.asm_().mov(r32(r2), asmjit::Imm(_offset));
			m_block.asm_().mov(r32(r3), asmjit::Imm(_inst));

			m_block.stack().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));
		}

		if (!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

		m_block.asm_().mov(r32(_dst.get()), r32(regReturnVal));
	}

	void Jitmem::readPeriph(DspValue& _dst, const EMemArea _area, const JitReg32& _offset, Instruction _inst) const
	{
		{
			const FuncArg r0(m_block, 0);
			const FuncArg r1(m_block, 1);
			const FuncArg r2(m_block, 2);
			const FuncArg r3(m_block, 3);

			m_block.asm_().mov(r64(r0), asmjit::Imm(&m_block.dsp()));
			m_block.asm_().mov(r32(r1), asmjit::Imm(_area == MemArea_Y ? 1 : 0));
			m_block.asm_().mov(r32(r2), _offset);
			m_block.asm_().mov(r32(r3), asmjit::Imm(_inst));

			m_block.stack().call(asmjit::func_as_ptr(&callDSPMemReadPeriph));
		}

		if (!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

		m_block.asm_().mov(r32(_dst.get()), r32(regReturnVal));
	}

	void Jitmem::readPeriph(DspValue& _dst, EMemArea _area, const DspValue& _offset, Instruction _inst) const
	{
		if (_offset.isImm24())
		{
			readPeriph(_dst, _area, _offset.imm24(), _inst);
		}
		else
		{
			if(JitStackHelper::isFuncArg(_offset.get(), 4))
			{
				const RegGP offset(m_block);
				m_block.asm_().mov(r32(offset), r32(_offset.get()));
				readPeriph(_dst, _area, r32(offset), _inst);
			}
			else
			{
				readPeriph(_dst, _area, r32(_offset), _inst);
			}
		}
	}

	void Jitmem::writePeriph(const EMemArea _area, const JitReg32& _offset, const DspValue& _value) const
	{
		const FuncArg r0(m_block, 0);
		const FuncArg r1(m_block, 1);
		const FuncArg r2(m_block, 2);
		const FuncArg r3(m_block, 3);

		// value might be held in a func arg register, safe if we set the func arg for the value first
		if (_value.isImmediate())
		{
			m_block.asm_().mov(r32(r3), asmjit::Imm(_value.imm()));
		}
		else
		{
			if (r32(r3) != r32(_value.get()))
				m_block.asm_().mov(r32(r3), r32(_value.get()));
		}

		m_block.asm_().mov(r64(r0), asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r32(r1), asmjit::Imm(_area == MemArea_Y ? 1 : 0));
		m_block.asm_().mov(r32(r2), _offset);

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
	}

	void Jitmem::writePeriph(const EMemArea _area, const TWord& _offset, const DspValue& _value) const
	{
		const FuncArg r0(m_block, 0);
		const FuncArg r1(m_block, 1);
		const FuncArg r2(m_block, 2);
		const FuncArg r3(m_block, 3);

		// value might be held in a func arg register, safe if we set the func arg for the value first
		if (_value.isImmediate())
		{
			m_block.asm_().mov(r32(r3), asmjit::Imm(_value.imm()));
		}
		else
		{
			if(r32(r3) != r32(_value.get()))
				m_block.asm_().mov(r32(r3), r32(_value.get()));
		}

		m_block.asm_().mov(r64(r0), asmjit::Imm(&m_block.dsp()));
		m_block.asm_().mov(r32(r1), asmjit::Imm(_area == MemArea_Y ? 1 : 0));
		m_block.asm_().mov(r32(r2), asmjit::Imm(_offset));

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
	}

	void Jitmem::writePeriph(EMemArea _area, const DspValue& _offset, const DspValue& _value) const
	{
		if (_offset.isImm24())
		{
			writePeriph(_area, _offset.imm24(), _value);
		}
		else
		{
			if(JitStackHelper::isFuncArg(_offset.get(), 4))
			{
				const RegGP temp(m_block);
				m_block.asm_().mov(r32(temp), r32(_offset.get()));
				writePeriph(_area, r32(temp), _value);
			}
			else
			{
				writePeriph(_area, r32(_offset.get()), _value);
			}
		}
	}

	const TWord* Jitmem::getMemAreaHostPtr(const EMemArea _area) const
	{
		const auto& mem = m_block.dsp().memory();

		switch (_area)
		{
		case MemArea_X:		return mem.x;
		case MemArea_Y:		return mem.y;
		case MemArea_P:		return mem.p;
		default:
			assert(0 && "invalid memory area");
			return nullptr;
		}
	}

	JitMemPtr Jitmem::getMemAreaPtr(ScratchPMem& _scratch, EMemArea _area, TWord offset, const ScratchPMem& _ptrToPmem) const
	{
		return getMemAreaPtr(_scratch, _area, offset, _ptrToPmem.isRegValid() ? r64(_ptrToPmem.get()) : JitReg64());
	}

	JitMemPtr Jitmem::getMemAreaPtr(ScratchPMem& _scratch, const EMemArea _area, TWord offset, const JitReg64& _ptrToPmem) const
	{
		const auto& mem = m_block.dsp().memory();

		const auto a = offset >= mem.getBridgedMemoryAddress() ? MemArea_P :_area;

		if(_ptrToPmem.isValid() && a == MemArea_P && !offset)
			return makePtr(_ptrToPmem, sizeof(TWord));

		const TWord* hostPtr = getMemAreaHostPtr(a) + offset;

		// try DSP regs pointer as base
		{
			auto p = m_block.dspRegPool().makeDspPtr(hostPtr, sizeof(TWord));

			if(p.hasSize())
				return p;
		}

		// if that didn't work and we have a valid P mem pointer, try relative to P memory
		if(_ptrToPmem.isValid())
		{
			const auto hostPtrP = getMemAreaHostPtr(MemArea_P);

			{
				const auto p = makeRelativePtr(hostPtr, hostPtrP, _ptrToPmem, sizeof(TWord));
				if(p.hasSize())
					return p;
			}

			// Failed, too. Create a new pointer relative to P memory in our scratch reg

			_scratch.temp(DspValue::Memory);
			const auto s = r64(_scratch);

			const auto off = reinterpret_cast<int64_t>(hostPtr) - reinterpret_cast<int64_t>(hostPtrP) + static_cast<int64_t>(offset) * sizeof(TWord);

			m_block.asm_().lea_(s, _ptrToPmem, static_cast<int>(off));

			return makePtr(s, sizeof(TWord));
		}

		// if that didn't work either we have to use absolute addressing
		_scratch.temp(DspValue::Memory);
		makeBasePtr(r64(_scratch.get()), hostPtr, sizeof(TWord));
		return makePtr(r64(_scratch), sizeof(TWord));
	}

	void Jitmem::getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset/* = 0*/, const JitRegGP& _ptrToPmem/* = JitRegGP()*/) const
	{
		const auto& mem = m_block.dsp().memory();

		const TWord* ptr = getMemAreaHostPtr(offset >= mem.getBridgedMemoryAddress() ? MemArea_P :_area);

		if(_ptrToPmem.isValid())
		{
			const auto off = reinterpret_cast<int64_t>(ptr) - reinterpret_cast<int64_t>(getMemAreaHostPtr(MemArea_P)) + static_cast<int64_t>(offset) * sizeof(TWord);

			m_block.asm_().lea_(_dst, r64(_ptrToPmem), static_cast<int>(off));
		}
		else
		{
			return makeBasePtr(_dst, ptr + offset, sizeof(TWord));
		}
	}

	JitMemPtr Jitmem::getMemAreaPtr(ScratchPMem& _dst, const EMemArea _area, const JitRegGP& _offset, ScratchPMem& _ptrToPmem) const
	{
		auto makeMemAreaPtr = [this, &_offset, &_dst](const EMemArea _a)
		{
#ifdef HAVE_X86_64
			const auto ptrHost = getMemAreaHostPtr(_a);
			const auto offset = reinterpret_cast<int64_t>(ptrHost) - reinterpret_cast<int64_t>(&m_block.dsp().regs());
			if(offset >= std::numeric_limits<int32_t>::min() && offset <= std::numeric_limits<int32_t>::max())
				return ptr(regDspPtr, _offset, 2, static_cast<int32_t>(offset), sizeof(TWord));
#endif
			return ptr(regDspPtr);
		};

		// as we bridge to P memory there is no need to do anything here if the requested area is P anyway
		if (_area == MemArea_P)
		{
			if(_ptrToPmem.isRegValid())
				return makePtr(r64(_ptrToPmem), _offset, 2, sizeof(TWord));

			const auto areaPtr = makeMemAreaPtr(_area);

			if(areaPtr.hasSize())
				return areaPtr;

			_dst.temp(DspValue::Memory);
			getMemAreaPtr(r64(_dst), _area);
		}
		else if(m_block.dsp().memory().hasMmuSupport())
		{
			const auto areaPtr = makeMemAreaPtr(_area);

			if(areaPtr.hasSize())
				return areaPtr;

			_dst.temp(DspValue::Memory);
			getMemAreaPtr(r64(_dst), _area);
		}
		else
		{
			// P memory is used for all bridged external memory
			auto& p = _ptrToPmem;
			if(!p.isRegValid())
			{
				p.temp(DspValue::Memory);
				getMemAreaPtr(r64(p), MemArea_P);
			}

			_dst.temp(DspValue::Memory);
			getMemAreaPtr(r64(_dst), _area);

			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().getBridgedMemoryAddress()));
#ifdef HAVE_ARM64
			m_block.asm_().csel(r64(_dst), r64(p), r64(_dst), asmjit::arm::CondCode::kGE);
#else
			m_block.asm_().cmovge(r64(_dst), r64(p));
#endif
		}

		return makePtr(r64(_dst), _offset, 2, sizeof(TWord));
	}

	void Jitmem::mov(const JitMemPtr& _dst, const DspValue& _src) const
	{
		if(_src.isImmediate())
		{
			mov(_dst, _src.imm());
		}
		else
		{
			assert((_src.getBitCount() == 8 && _dst.size() == sizeof(uint8_t)) || (_src.getBitCount() == 24 && _dst.size() == sizeof(TWord)) || (_src.getBitCount() >= 48 && _dst.size() == sizeof(uint64_t)));
			mov(_dst, _src.get());
		}
	}

	void Jitmem::mov(DspValue& _dst, const JitMemPtr& _src) const
	{
		assert(_dst.isRegValid());
		assert((_dst.getBitCount() == 8 && _src.size() == sizeof(uint8_t)) || (_dst.getBitCount() == 24 && _src.size() == sizeof(TWord)) || (_dst.getBitCount() >= 48 && _src.size() == sizeof(uint64_t)));

		mov(_dst.get(), _src);
	}

	void Jitmem::readDspMemory(DspValue& _dst, const JitMemPtr& _src) const
	{
		mov(r32(_dst.get()), _src);
	}

	void Jitmem::writeDspMemory(const JitMemPtr& _dst, const DspValue& _src) const
	{
		assert(_src.isRegValid());
		mov(_dst, r32(_src.get()));
	}

	void Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, const JitRegGP& _offset) const
	{
		ScratchPMem pmem(m_block, false, true);
		readDspMemory(_dst, _area, _offset, pmem);
	}
}
