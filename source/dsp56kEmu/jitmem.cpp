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
	void Jitmem::mov(uint32_t* _dst, const uint32_t& _imm) const
	{
		const RegGP src(m_block);
		m_block.asm_().mov(r32(src), asmjit::Imm(_imm));
		mov<sizeof(_imm)>(_dst, src);
	}

	void Jitmem::mov(uint64_t* _dst, const uint64_t& _imm) const
	{
		const RegGP src(m_block);
		m_block.asm_().mov(r64(src), asmjit::Imm(_imm));
		mov<sizeof(_imm)>(_dst, src);
	}

	template<size_t ByteSize>
	void Jitmem::mov(void* _dst, const JitRegGP& _src) const
	{
		const RegScratch reg(m_block);
		const auto p = makePtr(reg, _dst, ByteSize);
		mov<ByteSize>(p, _src);
	}

	void Jitmem::mov(uint64_t& _dst, const JitRegGP& _src) const
	{
		mov<sizeof(_dst)>(&_dst, _src);
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
			mov<sizeof(_dst)>(&_dst, _src.get());
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
			mov<sizeof(_dst)>(&_dst, _src.get());
		}
	}

	void Jitmem::mov(uint32_t& _dst, const JitReg128& _src) const
	{
		const RegScratch reg(m_block);
		m_block.asm_().movd(makePtr(reg, &_dst, sizeof(uint32_t)), _src);
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint64_t& _src) const
	{
		mov<sizeof(_src)>(_dst, makePtr(r64(_dst), &_src, sizeof(_src)));
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint32_t& _src) const
	{
		mov<sizeof(_src)>(_dst, makePtr(r64(_dst), &_src, sizeof(_src)));
	}

	void Jitmem::mov(const JitRegGP& _dst, const uint8_t& _src) const
	{
		mov<sizeof(_src)>(_dst, makePtr(r64(_dst), &_src, sizeof(_src)));
	}

	template<size_t ByteSize>
	void Jitmem::mov(const JitMemPtr& _dst, const JitRegGP& _src) const
	{
		static_assert(ByteSize == sizeof(TWord) || ByteSize == sizeof(uint64_t) || ByteSize == sizeof(uint8_t));

		switch (ByteSize)
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
			break;
		}
	}

	template void Jitmem::mov<1>(const JitMemPtr& _dst, const JitRegGP& _src) const;
	template void Jitmem::mov<4>(const JitMemPtr& _dst, const JitRegGP& _src) const;
	template void Jitmem::mov<8>(const JitMemPtr& _dst, const JitRegGP& _src) const;

	template<size_t ByteSize>
	void Jitmem::mov(const JitRegGP& _dst, const JitMemPtr& _src) const
	{
		static_assert(ByteSize == sizeof(TWord) || ByteSize == sizeof(uint64_t) || ByteSize == sizeof(uint8_t));

		switch (ByteSize)
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
			break;
		}
	}
	template void Jitmem::mov<1>(const JitRegGP& _dst, const JitMemPtr& _src) const;
	template void Jitmem::mov<4>(const JitRegGP& _dst, const JitMemPtr& _src) const;
	template void Jitmem::mov<8>(const JitRegGP& _dst, const JitMemPtr& _src) const;

	template<size_t ByteSize>
	void Jitmem::mov(const JitMemPtr& _dst, const uint64_t& _immSrc) const
	{
		const auto imm = asmjit::Imm(_immSrc);

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
		if (ByteSize <= sizeof(uint32_t))
			m_block.asm_().mov(r32(temp), imm);
		else
			m_block.asm_().mov(r64(temp), imm);
		mov<ByteSize>(_dst, temp);
#else
		auto ptr = _dst;
		ptr.setSize(ByteSize);
		m_block.asm_().mov(ptr, imm);
#endif
	}

	template void Jitmem::mov<1>(const JitMemPtr& _dst, const uint64_t& _immSrc) const;
	template void Jitmem::mov<4>(const JitMemPtr& _dst, const uint64_t& _immSrc) const;

	template <size_t ByteSize> void Jitmem::mov(const JitMemPtr& _dst, const DspValue& _src) const
	{
		if(_src.isImmediate())
		{
			mov<ByteSize>(_dst, _src.imm());
		}
		else
		{
			mov<ByteSize>(_dst, _src.get());
		}
	}

	template void Jitmem::mov<1>(const JitMemPtr& _dst, const DspValue& _src) const;
	template void Jitmem::mov<4>(const JitMemPtr& _dst, const DspValue& _src) const;

	void Jitmem::makeBasePtr(const JitReg64& _base, const void* _ptr, const size_t _size/* = sizeof(uint64_t)*/) const
	{
#ifdef HAVE_ARM64
		m_block.asm_().mov(_base, asmjit::Imm(_ptr));
#else
		const auto p = m_block.dspRegPool().makeDspPtr(_ptr, _size);
		if(isValid(p))
			m_block.asm_().lea(_base, p);
		else
			m_block.asm_().mov(_base, asmjit::Imm(reinterpret_cast<uint64_t>(_ptr)));
#endif
	}

	int32_t Jitmem::pointerOffset(const void* _target, const void* _source)
	{
		assert(_target != _source);

		const uint64_t target = reinterpret_cast<int64_t>(_target);
		const uint64_t source = reinterpret_cast<int64_t>(_source);

		const auto diffU = target - source;
		const auto diff = static_cast<int64_t>(diffU);

		if(diff > std::numeric_limits<int32_t>::max() || diff < std::numeric_limits<int32_t>::min())
			return 0;

		return static_cast<int32_t>(diff);
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _base, const JitRegGP& _index, const uint32_t _shift, const uint32_t _size)
	{
#ifdef HAVE_ARM64
		auto p = asmjit::a64::ptr(_base, _index, asmjit::arm::Shift(asmjit::arm::ShiftOp::kLSL, _shift));
		p.setSize(_size);
		return p;
#else
		return asmjit::x86::ptr(_base, _index, _shift, 0, _size);
#endif
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _base, const uint32_t _size)
	{
#ifdef HAVE_ARM64
		auto p = asmjit::a64::ptr(_base, 0);
		p.setSize(_size);
		return p;
#else
		return asmjit::x86::ptr(_base, 0, _size);
#endif
	}

	JitMemPtr Jitmem::makePtr(const JitReg64& _scratch, const void* _hostPtr, const uint32_t _size) const
	{
		const auto p = m_block.dspRegPool().makeDspPtr(_hostPtr, _size);

		if(isValid(p))
			return p;

		makeBasePtr(_scratch, _hostPtr, _size);
		return makePtr(_scratch, _size);
	}

	JitMemPtr Jitmem::makeRelativePtr(const void* _ptr, const void* _base, const JitReg64& _baseReg, const size_t _size)
	{
		if(_ptr == _base)
			return makePtr(_baseReg, static_cast<uint32_t>(_size));

		const auto offset = pointerOffset(_ptr, _base);
		if(!offset)
			return {};

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
		if(!canBeEncoded)
			return {};
#endif

		auto p = makePtr(_baseReg, static_cast<uint32_t>(_size));
		p.setOffset(offset);

		return p;
	}

	void Jitmem::makeDspPtr(const JitReg64& _dst) const
	{
		m_block.asm_().lea_(r64(_dst), r64(regDspPtr), &m_block.dsp(), &m_block.dsp().regs());
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dst, const EMemArea _area, const JitRegGP& _offset, MemoryRef&& _ref) const
	{
		const SkipLabel skip(m_block.asm_());

		if (!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

		auto p = getMemAreaPtr(_dst, _area, _offset, std::move(_ref));

		if(!hasMmuSupport())
		{
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
		}
		readDspMemory(_dst, p);

		return p;
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offsetX, const JitRegGP& _offsetY) const
	{
		auto px = readDspMemory(_dstX, MemArea_X, _offsetX, noRef());
		return readDspMemory(_dstY, MemArea_Y, _offsetY, std::move(px));
	}

	void Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offset) const
	{
		if (!_dstX.isRegValid())
			_dstX.temp(DspValue::Memory);
		if (!_dstY.isRegValid())
			_dstY.temp(DspValue::Memory);

		const SkipLabel skip(m_block.asm_());

		if(!hasMmuSupport())
		{
#ifdef HAVE_X86_64
			if (asmjit::Support::isPowerOf2(m_block.dsp().memory().sizeXY()))
			{
				// just return garbage in case memory is read from an invalid address
				m_block.asm_().and_(r32(_offset), asmjit::Imm(asmjit::Imm(m_block.dsp().memory().sizeXY() - 1)));
			}
			else
#endif
			{
				m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().sizeXY()));
				m_block.asm_().jge(skip.get());
			}
		}

		auto px = getMemAreaPtr(_dstX, MemArea_X, _offset, noRef());
		readDspMemory(_dstX, px);

		auto py = getMemAreaPtr(_dstY, MemArea_Y, _offset, std::move(px));
		readDspMemory(_dstY, py);
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const TWord& _offset) const
	{
		if (_offset >= m_block.dsp().memory().sizeXY())
			return noRef();

		if (!_dstX.isRegValid())
			_dstX.temp(DspValue::Memory);
		if (!_dstY.isRegValid())
			_dstY.temp(DspValue::Memory);

		auto memRefX = getMemAreaPtr(MemArea_X, _offset, noRef(), false);
		readDspMemory(_dstX, memRefX);

		auto memRefY = getMemAreaPtr(MemArea_Y, _offset, std::move(memRefX), false);
		readDspMemory(_dstY, memRefY);
		return memRefY;
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dst, const EMemArea _area, const TWord _offset) const
	{
		return readDspMemory(_dst, _area, _offset, noRef());
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset, MemoryRef&& _ref) const
	{
		const auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size(_area) && "memory address out of range");

		if (_offset >= mem.size(_area))
			return std::move(_ref);

		auto p = getMemAreaPtr(_area, _offset, std::move(_ref), false);

		if (!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

		readDspMemory(_dst, p);

		return p;
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dst, const EMemArea _area, const DspValue& _offset) const
	{
		return readDspMemory(_dst, _area, _offset, noRef());
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dst, const EMemArea _area, const DspValue& _offset, MemoryRef&& _ref) const
	{
		if (_offset.isType(DspValue::Immediate24))
			return readDspMemory(_dst, _area, _offset.imm24(), std::move(_ref));
		return readDspMemory(_dst, _area, _offset.get(), std::move(_ref));
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offsetX, const DspValue& _offsetY) const
	{
		if(_offsetX.isImm24() || _offsetY.isImm24())
		{
			MemoryRef ref(m_block);

			if (_offsetX.isImm24())
				ref = readDspMemory(_dstX, MemArea_X, _offsetX.imm24(), std::move(ref));
			else
				ref = readDspMemory(_dstX, MemArea_X, _offsetX.get(), std::move(ref));
			if (_offsetY.isImm24())
				ref = readDspMemory(_dstY, MemArea_Y, _offsetY.imm24(), std::move(ref));
			else
				ref = readDspMemory(_dstY, MemArea_Y, _offsetY.get(), std::move(ref));
			return ref;
		}

		return readDspMemory(_dstX, _dstY, _offsetX.get(), _offsetY.get());
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

	Jitmem::MemoryRef Jitmem::writeDspMemory(const EMemArea _area, const JitRegGP& _offset, const DspValue& _src, MemoryRef&& _ref) const
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
				makeDspPtr(r0);
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

				makeDspPtr(r0);
				m_block.asm_().mov(r32(r1), asmjit::Imm(_area));
			}

			m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWrite));

			return std::move(_ref);
		}
		else
		{
			DspValue tempXY(m_block);
			auto p = getMemAreaPtr(tempXY, _area, _offset, std::move(_ref));

			const SkipLabel skip(m_block.asm_());

			if(!hasMmuSupport())
			{
				m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().size(_area)));
				m_block.asm_().jge(skip.get());
			}

			writeDspMemory(p, _src);

			return p;
		}
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(EMemArea _area, const JitRegGP& _offset, const DspValue& _src) const
	{
		return writeDspMemory(_area, _offset, _src, noRef());
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(const JitRegGP& _offsetX, const JitRegGP& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const
	{
		auto px = writeDspMemory(MemArea_X, _offsetX, _srcX, noRef());
		auto py = writeDspMemory(MemArea_Y, _offsetY, _srcY, std::move(px));
		return py;
	}

	void Jitmem::writeDspMemory(const JitRegGP& _offset, const DspValue& _srcX, const DspValue& _srcY) const
	{
		const SkipLabel skip(m_block.asm_());

		if(!hasMmuSupport())
		{
			m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().sizeXY()));
			m_block.asm_().jge(skip.get());
		}

		DspValue tempXY(m_block);
		auto px = getMemAreaPtr(tempXY, MemArea_X, _offset, noRef());
		writeDspMemory(px, _srcX);

		auto py = getMemAreaPtr(tempXY, MemArea_Y, _offset, std::move(px));
		writeDspMemory(py, _srcY);
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(const TWord& _offset, const DspValue& _srcX, const DspValue& _srcY) const
	{
		if (_offset >= m_block.dsp().memory().sizeXY())
			return noRef();

		auto p = getMemAreaPtr(MemArea_X, _offset, noRef(), false);
		writeDspMemory(p, _srcX);

		// it makes no sense to write two values to the same memory address, which is the case for bridged memory. Skip second write in this case
		if(_offset >= m_block.dsp().memory().getBridgedMemoryAddress())
			return p;

		p = getMemAreaPtr(MemArea_Y, _offset, std::move(p), false);
		writeDspMemory(p, _srcY);
		return p;
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src) const
	{
		return writeDspMemory(_area, _offset, _src, noRef());
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src, MemoryRef&& _ref) const
	{
		if(m_block.getConfig().memoryWritesCallCpp || g_debugMemoryWrites)
		{
			const RegGP r(m_block);
			m_block.asm_().mov(r, asmjit::Imm(_offset));
			return writeDspMemory(_area, r.get(), _src, std::move(_ref));
		}

		const auto& mem = m_block.dsp().memory();
		mem.memTranslateAddress(_area, _offset);

		assert(_offset < mem.size(_area) && "memory address out of range");

		if(_offset >= mem.size(_area))
			return std::move(_ref);

		auto p = getMemAreaPtr(_area, _offset, std::move(_ref), false);
		writeDspMemory(p, _src);
		return p;
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(const EMemArea _area, const DspValue& _offset, const DspValue& _src) const
	{
		return writeDspMemory(_area, _offset, _src, noRef());
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src, MemoryRef&& _ref) const
	{
		if (_offset.isType(DspValue::Immediate24))
			return writeDspMemory(_area, _offset.imm24(), _src, std::move(_ref));
		return writeDspMemory(_area, _offset.get(), _src, std::move(_ref));
	}

	Jitmem::MemoryRef Jitmem::writeDspMemory(const DspValue& _offsetX, const DspValue& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const
	{
		if(_offsetX.isImm24() || _offsetY.isImm24())
		{
			auto r = noRef();
			if(_offsetX.isImm24())
				r = writeDspMemory(MemArea_X, _offsetX.imm24(), _srcX, std::move(r));
			else
				r = writeDspMemory(MemArea_X, _offsetX.get(), _srcX, std::move(r));

			if (_offsetY.isImm24())
				r = writeDspMemory(MemArea_Y, _offsetY.imm24(), _srcY, std::move(r));
			else
				r = writeDspMemory(MemArea_Y, _offsetY.get(), _srcY, std::move(r));
			return r;
		}

		return writeDspMemory(_offsetX.get(), _offsetY.get(), _srcX, _srcY);
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

			makeDspPtr(r0);
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

			auto assignArg = [this](const uint32_t _index, const JitRegGP& _d, const JitRegGP& _s)
			{
				if(_index == 0)
					makeDspPtr(_d.as<JitReg64>());
				else if(r32(_d) != r32(_s))
					m_block.asm_().mov(r32(_d), r32(_s));
			};

			assignFuncArgs({r0, r2}, {regDspPtr, _offset}, assignArg);

			m_block.asm_().mov(r32(r1), asmjit::Imm(_area == MemArea_Y ? 1 : 0));
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

		auto assignArg = [this](const uint32_t _index, const JitRegGP& _dst, const JitRegGP& _src)
		{
			if(_index == 0)
				makeDspPtr(_dst.as<JitReg64>());
			else if(r32(_dst) != r32(_src))
				m_block.asm_().mov(r32(_dst), r32(_src));
		};

		if (_value.isImmediate())
		{
			assignFuncArgs({r0.get(), r2.get()}, {regDspPtr, _offset.as<JitRegGP>()}, assignArg);

			m_block.asm_().mov(r32(r3), asmjit::Imm(_value.imm()));
		}
		else
		{
			assignFuncArgs({r0.get(), r2.get(), r3.get()}, {regDspPtr, _offset.as<JitRegGP>(), _value.get()}, assignArg);
		}

		m_block.asm_().mov(r32(r1), asmjit::Imm(_area == MemArea_Y ? 1 : 0));

		m_block.stack().call(asmjit::func_as_ptr(&callDSPMemWritePeriph));
	}

	void Jitmem::writePeriph(const EMemArea _area, const TWord& _offset, const DspValue& _value) const
	{
		const FuncArg r0(m_block, 0);
		const FuncArg r1(m_block, 1);
		const FuncArg r2(m_block, 2);
		const FuncArg r3(m_block, 3);
		
		if (_value.isImmediate())
		{
			makeDspPtr(r0);
			m_block.asm_().mov(r32(r3), asmjit::Imm(_value.imm()));
		}
		else
		{
			auto assignArg = [this](const uint32_t _index, const JitRegGP& _dst, const JitRegGP& _src)
			{
				if(_index == 0)
					makeDspPtr(_dst.as<JitReg64>());
				else if(r32(_dst) != r32(_src))
					m_block.asm_().mov(r32(_dst), r32(_src));
			};

			assignFuncArgs({r0, r3}, {regDspPtr, _value.get()}, assignArg);
		}

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

	Jitmem::MemoryRef Jitmem::noRef() const
	{
		return MemoryRef(m_block);
	}

	bool Jitmem::hasMmuSupport() const
	{
		return m_block.dsp().memory().hasMmuSupport();
	}

	void Jitmem::assignFuncArgs(const std::vector<JitRegGP>& _target, const std::vector<JitRegGP>& _source, const std::function<void(uint32_t, JitRegGP, JitRegGP)>&& _assignFunc)
	{
		assert(_target.size() == _source.size());

		std::unordered_map<uint32_t, JitRegGP> remainingSources;
		std::unordered_map<uint32_t, JitRegGP> remainingTargets;

		for(size_t i=0; i<_source.size(); ++i)
			remainingSources.insert({static_cast<uint32_t>(i), _source[i]});

		for(size_t i=0; i<_target.size(); ++i)
			remainingTargets.insert({static_cast<uint32_t>(i), _target[i]});

		auto equals = [&](const JitRegGP& _a, const JitRegGP& _b)
		{
			return r64(_a) == r64(_b);
		};

		auto isTarget = [&](const JitRegGP& _reg)
		{
			for (const auto& t : remainingTargets)
			{
				if(equals(t.second, _reg))
					return true;
			}
			return false;
		};

		auto isSource = [&](const JitRegGP& _reg)
		{
			for (const auto& s : remainingSources)
			{
				if(equals(s.second, _reg))
					return true;
			}
			return false;
		};

		// assign sources to targets as long as:
		// - the target that we want to assign to is not another source
		// - or target and source are equal registers anyway

		bool assignedSomething = true;

		while(assignedSomething)
		{
			assignedSomething = false;

			for(auto it = remainingSources.begin(); it != remainingSources.end();)
			{
				const auto index = it->first;
				const auto& source = it->second;

				if(equals(_target[index], _source[index]) || !isSource(_target[index]))
				{
					_assignFunc(index, _target[index], _source[index]);
					it = remainingSources.erase(it);
					remainingTargets.erase(index);
					assignedSomething = true;
				}
				else
				{
					++it;
				}
			}
		}

		assert(remainingTargets.empty() && remainingSources.empty());
	}

	Jitmem::MemoryRef Jitmem::getMemAreaPtr(const EMemArea _area, const TWord _offset, MemoryRef&& _ref, bool _supportIndexedAddressing) const
	{
		auto* hostPtr = getMemAreaHostPtr(_area) + _offset;

		// nothing to do if _ref is already pointing to it
		if(hostPtr == _ref.baseAddr)
			return std::move(_ref);

		JitMemPtr ptr;

		if(!_ref.isValid())
		{
			ptr = makeRelativePtr(hostPtr, &m_block.dsp().regs(), regDspPtr, 4);
		}
		else
		{
			ptr = makeRelativePtr(hostPtr, _ref.baseAddr, _ref.reg, 4);
		}

		if(isValid(ptr))
		{
#ifdef HAVE_ARM64
			// ARM does not have an addressing mode that supports both an index register and an offset
			if(_supportIndexedAddressing && ptr.hasOffset())
				return copyHostAddressToReg(_area, _offset, _ref);
#endif
			MemoryRef m(std::move(_ref));
			m.ptr = ptr;
			return m;
		}

		return copyHostAddressToReg(_area, _offset, _ref);
	}

	Jitmem::MemoryRef Jitmem::getMemAreaPtr(const EMemArea _area, const JitRegGP& _offset, MemoryRef&& _ref) const
	{
		DspValue tempXY(m_block);
		return getMemAreaPtr(tempXY, _area, _offset, std::move(_ref));
	}

	Jitmem::MemoryRef Jitmem::getMemAreaPtr(DspValue& _tempXY, EMemArea _area, const JitRegGP& _offset,	MemoryRef&& _ref) const
	{
		if(_area == MemArea_P || hasMmuSupport())
		{
			auto m = getMemAreaPtr(_area, 0, std::move(_ref), true);
			m.ptr.setIndex(_offset, 2);
			return m;
		}

		// P memory is used for all bridged external memory

		auto* hostPtrP = getMemAreaHostPtr(MemArea_P);
		auto* hostPtrXY = getMemAreaHostPtr(_area);

		DspValue tempXY(m_block);
		DspValue tempP(m_block, false, true);

		JitReg64 gpXY, gpP;

		if(_ref.baseAddr == hostPtrP)
			gpP = _ref.reg;
		else if(_ref.baseAddr == hostPtrXY)
			gpXY = _ref.reg;

		if(!gpP.isValid())
		{
			copyHostAddressToReg(tempP, MemArea_P, 0, _ref);
			gpP = r64(tempP);
		}

		if(!gpXY.isValid())
		{
			// handle edge cases such as move y:(r2),r2, in this case the provided temporary is equal to _offset
			if(!_tempXY.isRegValid() || r64(_tempXY) == r64(_offset))
			{
				tempXY.temp(DspValue::Memory);
				gpXY = r64(tempXY);
			}
			else
			{
				gpXY = r64(_tempXY);
			}
			m_block.asm_().lea_(gpXY, gpP, pointerOffset(hostPtrXY, hostPtrP));
		}

		m_block.asm_().cmp(r32(_offset), asmjit::Imm(m_block.dsp().memory().getBridgedMemoryAddress()));
#ifdef HAVE_ARM64
		m_block.asm_().csel(r64(gpXY), r64(gpP), r64(gpXY), asmjit::arm::CondCode::kGE);
#else
		m_block.asm_().cmovge(r64(gpXY), r64(gpP));
#endif

		_ref.reset();

		auto r = noRef();

		assert(r64(gpXY) != r64(_offset));

		r.ptr.setBase(gpXY);
		r.ptr.setIndex(_offset, 2);
		r.ptr.setSize(sizeof(TWord));

		return r;
	}

	void Jitmem::copyHostAddressToReg(DspValue& _dst, const EMemArea _area, const TWord _offset, const MemoryRef& _ref) const
	{
		const auto& mem = m_block.dsp().memory();

		const TWord* ptr = getMemAreaHostPtr(_offset >= mem.getBridgedMemoryAddress() ? MemArea_P :_area) + _offset;

		if(ptr == _ref.baseAddr)
		{
			if(r64(_dst) == r64(_ref.reg))
				return;

			if(!_dst.isRegValid())
			{
				_dst.temp(DspValue::Memory);
				return;
			}

			assert(!_ref.value.isRegValid() && "hey caller, use _ref directly");
			m_block.asm_().mov(r64(_dst), _ref.reg);
			return;
		}

		if(!_dst.isRegValid())
			_dst.temp(DspValue::Memory);

		// try relative to memory reference
		if(_ref.isValid())
		{
			const auto off = pointerOffset(ptr, _ref.baseAddr);
			if(off)
			{
				assert(r64(_dst) != r64(_ref.reg));	// will not work on ARM

				if(!m_block.asm_().lea_(r64(_dst), r64(_ref.reg), off))
				{
					_dst.temp(DspValue::Memory);
					const auto r = m_block.asm_().lea_(r64(_dst), r64(_ref.reg), off);
					assert(r);
				}
				return;
			}
		}

		// try dsp reg
		const auto off = pointerOffset(ptr, &m_block.dsp().regs());
		if(off)
		{
			m_block.asm_().lea_(r64(_dst), r64(regDspPtr), off);
			return;
		}

		// didn't work either, copy absolute address to register
		return makeBasePtr(r64(_dst), ptr, sizeof(TWord));
	}

	Jitmem::MemoryRef Jitmem::copyHostAddressToReg(const EMemArea _area, const TWord _offset, const MemoryRef& _ref) const
	{
		MemoryRef m(m_block);

		copyHostAddressToReg(m.value, _area, _offset, _ref);

		m.reg = r64(m.value);
		m.baseAddr = getMemAreaHostPtr(_area) + _offset;
		m.ptr = makePtr(m.reg, sizeof(TWord));

		return m;
	}

	template<size_t ByteSize>
	void Jitmem::mov(DspValue& _dst, const JitMemPtr& _src) const
	{
		assert(_dst.isRegValid());

		switch (ByteSize)
		{
		case sizeof(uint8_t):
			mov<sizeof(uint8_t)>(_dst.get(), _src);
			break;
		case sizeof(uint32_t):
			assert(_dst.getBitCount() == 24);
			mov<sizeof(TWord)>(_dst.get(), _src);
			break;
		default:
			assert(_dst.getBitCount() >= 48);
			mov<sizeof(uint64_t)>(_dst.get(), _src);
		}
	}

	template void Jitmem::mov<1>(DspValue& _dst, const JitMemPtr& _src) const;
	template void Jitmem::mov<4>(DspValue& _dst, const JitMemPtr& _src) const;

	void Jitmem::readDspMemory(DspValue& _dst, const MemoryRef& _src) const
	{
		mov<sizeof(TWord)>(_dst, _src.ptr);
	}

	void Jitmem::writeDspMemory(const JitMemPtr& _dst, const DspValue& _src) const
	{
		assert(_src.isRegValid());
		mov<sizeof(TWord)>(_dst, r32(_src.get()));
	}

	void Jitmem::writeDspMemory(const MemoryRef& _dst, const DspValue& _src) const
	{
		writeDspMemory(_dst.ptr, _src);
	}

	Jitmem::MemoryRef Jitmem::readDspMemory(DspValue& _dst, EMemArea _area, const JitRegGP& _offset) const
	{
		return readDspMemory(_dst, _area, _offset, noRef());
	}
}
