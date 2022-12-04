#pragma once

#include "jitregtracker.h"
#include "jitregtypes.h"
#include "opcodetypes.h"
#include "types.h"

namespace dsp56k
{
	class DspValue;
	class JitBlock;

	class Jitmem
	{
	public:
		using ScratchPMem = DspValue;

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

		static JitMemPtr makePtr(const JitReg64& _base, const JitRegGP& _index, uint32_t _shift = 0, uint32_t _size = 0);
		static JitMemPtr makePtr(const JitReg64& _base, uint32_t _size);

		JitMemPtr makePtr(const JitReg64& _scratch, const void* _hostPtr, uint32_t _size) const;

		static JitMemPtr makeRelativePtr(const void* _ptr, const void* _base, const JitReg64& _baseReg, size_t _size);

		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const TWord& _offset) const;
		void readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset) const;
		void readDspMemory(DspValue& _dst, EMemArea _area, TWord _offset, ScratchPMem& _basePtrPmem) const;

		void readDspMemory(DspValue& _dst, EMemArea _area, const DspValue& _offset) const;
		void readDspMemory(DspValue& _dst, EMemArea _area, const DspValue& _offset, ScratchPMem& _basePtrPmem) const;
		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offsetX, const DspValue& _offsetY) const;
		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const DspValue& _offset) const;
		
		void writeDspMemory(const TWord& _offset, const DspValue& _srcX, const DspValue& _srcY) const;
		void writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src) const;
		void writeDspMemory(EMemArea _area, TWord _offset, const DspValue& _src, ScratchPMem& _basePtrPmem) const;

		void writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src) const;
		void writeDspMemory(EMemArea _area, const DspValue& _offset, const DspValue& _src, ScratchPMem& _basePtrPmem) const;
		void writeDspMemory(const DspValue& _offsetX, const DspValue& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const;
		void writeDspMemory(const DspValue& _offset, const DspValue& _srcX, const DspValue& _srcY) const;

		void readPeriph(DspValue& _dst, EMemArea _area, const TWord& _offset, Instruction _inst) const;
		void readPeriph(DspValue& _dst, EMemArea _area, const DspValue& _offset, Instruction _inst) const;

		void writePeriph(EMemArea _area, const TWord& _offset, const DspValue& _value) const;
		void writePeriph(EMemArea _area, const DspValue& _offset, const DspValue& _value) const;

		void mov(const JitMemPtr& _dst, const DspValue& _src) const;
		void mov(DspValue& _dst, const JitMemPtr& _src) const;

		void readDspMemory(DspValue& _dst, const JitMemPtr& _src) const;
		void writeDspMemory(const JitMemPtr& _dst, const DspValue& _src) const;

		JitMemPtr getMemAreaPtr(ScratchPMem& _scratch, EMemArea _area, TWord offset, const ScratchPMem& _ptrToPmem) const;
		JitMemPtr getMemAreaPtr(ScratchPMem& _scratch, EMemArea _area, TWord offset, const JitReg64& _ptrToPmem = JitReg64()) const;
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset = 0, const JitRegGP& _ptrToPmem = JitRegGP()) const;
		JitMemPtr getMemAreaPtr(ScratchPMem& _dst, EMemArea _area, const JitRegGP& _offset, ScratchPMem& _ptrToPmem) const;

	private:
		void readDspMemory(DspValue& _dst, EMemArea _area, const JitRegGP& _offset) const;
		void readDspMemory(DspValue& _dst, EMemArea _area, const JitRegGP& _offset, ScratchPMem& _basePtrPmem) const;
		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offsetX, const JitRegGP& _offsetY) const;
		void readDspMemory(DspValue& _dstX, DspValue& _dstY, const JitRegGP& _offset) const;

		void writeDspMemory(EMemArea _area, const JitRegGP& _offset, const DspValue& _src, ScratchPMem& _basePtrPmem) const;
		void writeDspMemory(EMemArea _area, const JitRegGP& _offset, const DspValue& _src) const;
		void writeDspMemory(const JitRegGP& _offsetX, const JitRegGP& _offsetY, const DspValue& _srcX, const DspValue& _srcY) const;
		void writeDspMemory(const JitRegGP& _offset, const DspValue& _srcX, const DspValue& _srcY) const;

		void readPeriph(DspValue& _dst, EMemArea _area, const JitReg32& _offset, Instruction _inst) const;

		void writePeriph(EMemArea _area, const JitReg32& _offset, const DspValue& _value) const;

		const TWord* getMemAreaHostPtr(EMemArea _area) const;

		JitBlock& m_block;
	};
}
