#pragma once

#include "jitregtracker.h"
#include "jitregtypes.h"
#include "types.h"

namespace dsp56k
{
	class JitBlock;

	class Jitmem
	{
	public:
		Jitmem(JitBlock& _block) : m_block(_block) {}

		void mov(TReg24& _dst, const JitReg128& _src);
		void mov(TReg48& _dst, const JitReg128& _src);
		void mov(TReg56& _dst, const JitReg128& _src);

		void mov(const JitReg128& _dst, TReg24& _src);
		void mov(const JitReg128& _dst, TReg56& _src);
		void mov(const JitReg128& _dst, TReg48& _src);

		void mov(const JitRegGP& _dst, const TReg24& _src);
		void mov(const JitRegGP& _dst, const TReg48& _src);
		void mov(const JitRegGP& _dst, const TReg56& _src);

		void mov(const TReg24& _dst, const JitRegGP& _src);
		void mov(const TReg48& _dst, const JitRegGP& _src);
		void mov(const TReg56& _dst, const JitRegGP& _src);

		void mov(uint64_t& _dst, const JitRegGP& _src) const;
		void mov(uint32_t& _dst, const JitRegGP& _src) const;
		void mov(uint8_t& _dst, const JitRegGP& _src) const;

		void mov(const JitRegGP& _dst, const uint64_t& _src) const;
		void mov(const JitRegGP& _dst, const uint32_t& _src) const;
		void mov(const JitRegGP& _dst, const uint8_t& _src) const;

		void mov(void* _dst, void* _src, uint32_t _size);
		void mov(void* _dst, const JitRegGP& _src, uint32_t _size);

		template<typename T, unsigned int B>
		JitMemPtr ptr(const JitReg64& _temp, const RegType<T, B>& _reg)
		{
			return ptr<T>(_temp, &_reg.var);
		}

		template<typename T>
		JitMemPtr ptr(const JitReg64& _temp, const T* _t) const;

		template<typename T>
		void ptrToReg(const JitReg64& _r, const T* _t) const;

		static JitMemPtr makePtr(const JitReg64& _base, const JitRegGP& _index, uint32_t _shift = 0, int32_t _offset = 0, uint32_t _size = 0);
		static JitMemPtr makePtr(const JitReg64& _base, uint32_t _offset, uint32_t _size);

		static void setPtrOffset(JitMemPtr& _mem, const void* _base, const void* _member);
		
		void readDspMemory(const JitRegGP& _dst, EMemArea _area, const JitRegGP& _offset) const;
		void writeDspMemory(EMemArea _area, const JitRegGP& _offset, const JitRegGP& _src) const;

		void readDspMemory(const JitRegGP& _dst, EMemArea _area, TWord _offset) const;
		void writeDspMemory(EMemArea _area, TWord _offset, const JitRegGP& _src) const;

		void readPeriph(const JitReg64& _dst, EMemArea _area, const TWord& _offset) const;
		void readPeriph(const JitReg64& _dst, EMemArea _area, const JitReg64& _offset) const;
		void writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const;
		void writePeriph(EMemArea _area, const TWord& _offset, const JitReg64& _value) const;

	private:
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset = 0) const;
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, const JitRegGP& _offset) const;
		JitBlock& m_block;
	};
}
