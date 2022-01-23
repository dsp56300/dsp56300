#pragma once

#include "jitregtracker.h"
#include "jitregtypes.h"
#include "opcodetypes.h"
#include "types.h"

namespace dsp56k
{
	class JitBlock;

	class Jitmem
	{
	public:
		Jitmem(JitBlock& _block) : m_block(_block) {}

		void mov(uint64_t& _dst, const JitRegGP& _src) const;
		void mov(uint32_t& _dst, const JitRegGP& _src) const;
		void mov(uint8_t& _dst, const JitRegGP& _src) const;

		void mov(uint32_t& _dst, const JitReg128& _src) const;

		void mov(const JitRegGP& _dst, const uint64_t& _src) const;
		void mov(const JitRegGP& _dst, const uint32_t& _src) const;
		void mov(const JitRegGP& _dst, const uint8_t& _src) const;

		template<typename T, unsigned int B>
		JitMemPtr ptr(const JitReg64& _temp, const RegType<T, B>& _reg)
		{
			return ptr<T>(_temp, &_reg.var);
		}

		template<typename T>
		JitMemPtr ptr(const JitReg64& _temp, const T* _t) const;

		template<typename T>
		void ptrToReg(const JitReg64& _r, const T* _t) const;

		void makeBasePtr(const JitReg64& _base, const void* _ptr) const;

		static JitMemPtr makePtr(const JitReg64& _base, const JitRegGP& _index, uint32_t _shift = 0, uint32_t _size = 0);
		static JitMemPtr makePtr(const JitReg64& _base, uint32_t _offset, uint32_t _size);

		void readDspMemory(const JitRegGP& _dst, EMemArea _area, const JitRegGP& _offset, const JitReg64& _basePtrPmem = JitReg64()) const;
		void readDspMemory(const JitRegGP& _dstX, const JitRegGP& _dstY, const JitRegGP& _offsetX, const JitRegGP& _offsetY) const;
		void readDspMemory(const JitRegGP& _dstX, const JitRegGP& _dstY, const JitRegGP& _offset) const;
		void readDspMemory(const JitRegGP& _dstX, const JitRegGP& _dstY, const TWord& _offset) const;
		void readDspMemory(const JitRegGP& _dst, EMemArea _area, TWord _offset) const;

		void writeDspMemory(EMemArea _area, const JitRegGP& _offset, const JitRegGP& _src, const JitReg64& _basePtrPmem = JitReg64()) const;
		void writeDspMemory(const JitRegGP& _offsetX, const JitRegGP& _offsetY, const JitRegGP& _srcX, const JitRegGP& _srcY) const;
		void writeDspMemory(const JitRegGP& _offset, const JitRegGP& _srcX, const JitRegGP& _srcY) const;
		void writeDspMemory(const TWord& _offset, const JitRegGP& _srcX, const JitRegGP& _srcY) const;
		void writeDspMemory(EMemArea _area, TWord _offset, const JitRegGP& _src) const;

		void readPeriph(const JitReg64& _dst, EMemArea _area, const TWord& _offset, Instruction _inst) const;
		void readPeriph(const JitReg64& _dst, EMemArea _area, const JitReg64& _offset, Instruction _inst) const;
		void writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const;
		void writePeriph(EMemArea _area, const TWord& _offset, const JitReg64& _value) const;

	private:
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset = 0, const JitRegGP& _ptrToPmem = JitRegGP()) const;
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, const JitRegGP& _offset, const JitReg64& _ptrToPmem = JitReg64()) const;
		JitBlock& m_block;
	};
}
