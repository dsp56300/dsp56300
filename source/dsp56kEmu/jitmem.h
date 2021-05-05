#pragma once

#include "types.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	class Jitmem
	{
	public:
		template<typename T, unsigned int B>
		static asmjit::x86::Mem ptr(asmjit::x86::Assembler& _a, RegType<T, B>& _reg)
		{
			return ptr<T>(_a, &_reg.var);
		}

		template<typename T>
		static asmjit::x86::Mem ptr(asmjit::x86::Assembler& _a, const T* _t)
		{
			if constexpr (sizeof(T*) == sizeof(uint64_t))
				_a.mov(asmjit::x86::rax, reinterpret_cast<uint64_t>(_t));
			else if constexpr (sizeof(T*) == sizeof(uint32_t))
				_a.mov(asmjit::x86::rax, reinterpret_cast<uint32_t>(_t));
			else
				static_assert(sizeof(T*) == sizeof(uint64_t) || sizeof(T*) == sizeof(uint32_t), "unknown pointer size");

			return asmjit::x86::ptr(asmjit::x86::rax);
		}		
	};
}
