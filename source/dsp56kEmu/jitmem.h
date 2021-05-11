#pragma once

#include "jitregtracker.h"
#include "types.h"

#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	class JitBlock;

	class Jitmem
	{
	public:
		Jitmem(JitBlock& _block) : m_block(_block) {}

		void mov(TReg24& _dst, const asmjit::x86::Xmm& _src);
		void mov(TReg48& _dst, const asmjit::x86::Xmm& _src);
		void mov(TReg56& _dst, const asmjit::x86::Xmm& _src);

		void mov(const asmjit::x86::Xmm& _dst, TReg24& _src);
		void mov(const asmjit::x86::Xmm& _dst, TReg56& _src);
		void mov(const asmjit::x86::Xmm& _dst, TReg48& _src);

		void mov(const asmjit::x86::Gp& _dst, TReg24& _src);
		void mov(TReg24& _dst, const asmjit::x86::Gp& _src);

		void mov(uint64_t& _dst, const asmjit::x86::Gp& _src) const;
		void mov(uint32_t& _dst, const asmjit::x86::Gp& _src) const;

		void mov(const asmjit::x86::Gp& _dst, uint64_t& _src) const;
		void mov(const asmjit::x86::Gp& _dst, uint32_t& _src) const;

		template<typename T, unsigned int B>
		asmjit::x86::Mem ptr(const asmjit::x86::Gpq& _temp, RegType<T, B>& _reg)
		{
			return ptr<T>(_temp, &_reg.var);
		}

		template<typename T>
		asmjit::x86::Mem ptr(const asmjit::x86::Gpq& _temp, const T* _t) const;

		template<typename T>
		void ptrToReg(const asmjit::x86::Gpq& _r, const T* _t) const;

		void getOpWordB(RegGP& _dst) const;

		void readPeriph(const asmjit::x86::Gpq& _dst, EMemArea _area, const asmjit::x86::Gpq& _offset) const;
		void writePeriph(EMemArea _area, const asmjit::x86::Gpq& _offset, const asmjit::x86::Gpq& _value) const;

	private:
		JitBlock& m_block;
	};
}
