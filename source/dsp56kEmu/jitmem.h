#pragma once

#include "types.h"

#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	class Jithelper
	{
	public:
		template<typename T, size_t B>
		static asmjit::x86::Mem ptr(const RegType<T, B>& _reg)
		{
			return ptr<T>(&_reg.var);
		}

		template<typename T>
		static asmjit::x86::Mem ptr(const T* _t)
		{
			if constexpr (sizeof(T*) == sizeof(uint64_t))
				return asmjit::x86::Mem(reinterpret_cast<uint64_t>(_t));
			else if constexpr (sizeof(T*) == sizeof(uint32_t))
				return asmjit::x86::Mem(reinterpret_cast<uint32_t>(_t));
			else
				static_assert(false, "unknown pointer size");
		}		
	};
}
