#pragma once

#include "jitregtracker.h"
#include "jittypes.h"
#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

#include "asmjit/x86/x86operand.h"

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

		void mov(const asmjit::x86::Gp& _dst, TReg24& _src);
		void mov(TReg24& _dst, const asmjit::x86::Gp& _src);

		void mov(uint64_t& _dst, const asmjit::x86::Gp& _src) const;
		void mov(uint32_t& _dst, const asmjit::x86::Gp& _src) const;
		void mov(uint8_t& _dst, const asmjit::x86::Gp& _src) const;

		void mov(const asmjit::x86::Gp& _dst, const uint64_t& _src) const;
		void mov(const asmjit::x86::Gp& _dst, const uint32_t& _src) const;
		void mov(const asmjit::x86::Gp& _dst, const uint8_t& _src) const;

		template<typename T, unsigned int B>
		asmjit::x86::Mem ptr(const JitReg64& _temp, RegType<T, B>& _reg)
		{
			return ptr<T>(_temp, &_reg.var);
		}

		template<typename T>
		asmjit::x86::Mem ptr(const JitReg64& _temp, const T* _t) const;

		template<typename T>
		void ptrToReg(const JitReg64& _r, const T* _t) const;

		void readDspMemory(const JitReg& _dst, EMemArea _area, const JitReg& _offset) const;
		void writeDspMemory(EMemArea _area, const JitReg& _offset, const JitReg& _src) const;

		void readDspMemory(const JitReg& _dst, EMemArea _area, TWord _offset) const;
		void writeDspMemory(EMemArea _area, TWord _offset, const JitReg& _src) const;

		void readPeriph(const JitReg64& _dst, EMemArea _area, const TWord& _offset) const;
		void readPeriph(const JitReg64& _dst, EMemArea _area, const JitReg64& _offset) const;
		void writePeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value) const;
		void writePeriph(EMemArea _area, const TWord& _offset, const JitReg64& _value) const;

	private:
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, TWord offset = 0) const;
		void getMemAreaPtr(const JitReg64& _dst, EMemArea _area, const JitReg& _offset) const;
		JitBlock& m_block;
	};
}
