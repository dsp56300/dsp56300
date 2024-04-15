#pragma once

#include "jitdspvalue.h"
#include "jitregtracker.h"
#include "opcodetypes.h"
#include "types.h"

namespace dsp56k
{
	class DspValue;
	class JitBlock;

	class Jitmem
	{
	public:
		struct MemoryRef
		{
			DspValue value;						// may hold the base register if applicable
			JitReg64 reg;						// base register
			JitMemPtr ptr;						// offset from base register to target memory location
			const void* baseAddr = nullptr;		// host memory address that the base register is pointing to

			bool isValid() const { return baseAddr != nullptr && reg.isValid(); }

			void reset()
			{
				value.release();
				reg.reset();
				ptr.reset();
				baseAddr = nullptr;
			}

			explicit MemoryRef(JitBlock& _block) : value(_block)
			{
			}

			MemoryRef(MemoryRef&& _ref) noexcept
			: value(std::move(_ref.value))
			, reg(_ref.reg)
			, ptr(_ref.ptr)
			, baseAddr(_ref.baseAddr)
			{
				_ref.reset();
			}

			MemoryRef& operator = (const MemoryRef&) = delete;
			MemoryRef& operator = (MemoryRef&& _ref) noexcept
			{
				value = std::move(_ref.value);
				reg = _ref.reg;
				ptr = _ref.ptr;
				baseAddr = _ref.baseAddr;

				_ref.reset();

				return *this;
			}
		};

		Jitmem(JitBlock& _block) : m_block(_block) {}

		void mov(void* _dst, const uint32_t& _imm) const;
		void mov(void* _dst, const uint64_t& _imm) const;
		void mov(void* _dst, const JitRegGP& _src, uint32_t _size) const;
		void mov(uint64_t& _dst, const JitRegGP& _src) const;
		void mov(uint64_t& _dst, const DspValue& _src) const;
		void mov(uint32_t& _dst, const DspValue& _src) const;

		void mov(uint32_t& _dst, const JitReg128& _src) const;

		void mov(const JitRegGP& _dst, const uint64_t& _src) const;
		void mov(const JitRegGP& _dst, const uint32_t& _src) const;
		void mov(const JitRegGP& _dst, const uint8_t& _src) const;

		void mov(const JitMemPtr& _dst, const JitRegGP& _src) const;
		void mov(const JitRegGP& _dst, const JitMemPtr& _src) const;

		void mov(const JitMemPtr& _dst, const uint64_t& _immSrc) const;

		void makeBasePtr(const JitReg64& _base, const void* _ptr, size_t _size = sizeof(uint64_t)) const;

		static int32_t pointerOffset(const void* _target, const void* _source);

		static JitMemPtr makePtr(const JitReg64& _base, const JitRegGP& _index, uint32_t _shift = 0, uint32_t _size = 0);
		static JitMemPtr makePtr(const JitReg64& _base, uint32_t _size);

		JitMemPtr makePtr(const JitReg64& _scratch, const void* _hostPtr, uint32_t _size) const;

		static JitMemPtr makeRelativePtr(const void* _ptr, const void* _base, const JitReg64& _baseReg, size_t _size);

		void makeDspPtr(const JitReg64& _dst) const;

		MemoryRef readDspMemory(DspValue& _dstX, DspValue& _dstY, const TWord& _offset) const;
		MemoryRef readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset) const;
		MemoryRef readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset, MemoryRef&& _ref) const;

		MemoryRef readDspMemory(DspValue& _dst, EMemArea _area, const DspValue& _offset) const;
		MemoryRef readDspMemory(DspValue& _dst, EMemArea _area, const DspValue& _offset, MemoryRef&& _ref) const;
		MemoryRef readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offsetX, const DspValue& _offsetY) const;
		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offset) const;
		
		MemoryRef writeDspMemory(const TWord& _offset, const DspValue& _srcX, const DspValue& _srcY) const;
		MemoryRef writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src) const;
		MemoryRef writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src, MemoryRef&& _ref) const;

		MemoryRef writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src) const;
		MemoryRef writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src, MemoryRef&& _ref) const;
		MemoryRef writeDspMemory(const DspValue& _offsetX, const DspValue& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const;
		void writeDspMemory(const DspValue& _offset, const DspValue& _srcX, const DspValue& _srcY) const;

		void readPeriph(DspValue& _dst, EMemArea _area, const TWord& _offset, Instruction _inst) const;
		void readPeriph(DspValue& _dst, EMemArea _area, const DspValue& _offset, Instruction _inst) const;

		void writePeriph(EMemArea _area, const TWord& _offset, const DspValue& _value) const;
		void writePeriph(EMemArea _area, const DspValue& _offset, const DspValue& _value) const;

		void mov(const JitMemPtr& _dst, const DspValue& _src) const;
		void mov(DspValue& _dst, const JitMemPtr& _src) const;

		void readDspMemory(DspValue& _dst, const JitMemPtr& _src) const;
		void readDspMemory(DspValue& _dst, const MemoryRef& _src) const;
		void writeDspMemory(const JitMemPtr& _dst, const DspValue& _src) const;
		void writeDspMemory(const MemoryRef& _dst, const DspValue& _src) const;

		MemoryRef getMemAreaPtr(EMemArea _area, TWord _offset, MemoryRef&& _ref, bool _supportIndexedAddressing) const;
		MemoryRef getMemAreaPtr(EMemArea _area, const JitRegGP& _offset, MemoryRef&& _ref) const;
		MemoryRef getMemAreaPtr(DspValue& _tempXY, EMemArea _area, const JitRegGP& _offset, MemoryRef&& _ref) const;

	private:
		void copyHostAddressToReg(DspValue& _dst, EMemArea _area, TWord _offset, const MemoryRef& _ref) const;
		MemoryRef copyHostAddressToReg(EMemArea _area, TWord _offset, const MemoryRef& _ref) const;

		MemoryRef readDspMemory(DspValue& _dst, EMemArea _area, const JitRegGP& _offset) const;
		MemoryRef readDspMemory(DspValue& _dst, EMemArea _area, const JitRegGP& _offset, MemoryRef&& _ref) const;
		MemoryRef readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offsetX, const JitRegGP& _offsetY) const;
		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offset) const;

		MemoryRef writeDspMemory(EMemArea _area, const JitRegGP& _offset, const DspValue& _src, MemoryRef&& _ref) const;
		MemoryRef writeDspMemory(EMemArea _area, const JitRegGP& _offset, const DspValue& _src) const;
		MemoryRef writeDspMemory(const JitRegGP& _offsetX, const JitRegGP& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const;
		void writeDspMemory(const JitRegGP& _offset, const DspValue& _srcX, const DspValue& _srcY) const;

		void readPeriph(DspValue& _dst, EMemArea _area, const JitReg32& _offset, Instruction _inst) const;

		void writePeriph(EMemArea _area, const JitReg32& _offset, const DspValue& _value) const;

		const TWord* getMemAreaHostPtr(EMemArea _area) const;

		MemoryRef noRef() const;

		bool hasMmuSupport() const;

		JitBlock& m_block;
	};
}
