#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <vector>

#include "jittypes.h"

namespace asmjit
{
	inline namespace _abi_1_9
	{
		class BaseNode;
	}
}

namespace dsp56k
{
	class JitBlock;

	class JitStackHelper
	{
	public:
		JitStackHelper(JitBlock& _block);
		~JitStackHelper();

		void push(const JitReg64& _reg);
		void push(const JitReg128& _reg);

		void pop(const JitReg64& _reg);
		void pop(const JitReg128& _reg);
		void pop(const JitReg& _reg);
		void pop();

		void popAll();

		void pushNonVolatiles();
		
		void call(const void* _funcAsPtr, bool _isJitCall = false);
		void call(const std::function<void()>& _execCall, bool _isJitCall = false);

		void pushAllUsed(asmjit::BaseNode* _baseNode);

		static bool isFuncArg(const JitRegGP& _gp, uint32_t _maxIndex = 255);
		static bool isNonVolatile(const JitReg& _gp);
		static bool isNonVolatile(const JitRegGP& _gp);
		static bool isNonVolatile(const JitReg128& _xm);

		void setUsed(const JitReg& _reg);
		void setUsed(const JitRegGP& _reg);
		void setUsed(const JitReg128& _reg);

		const auto& getUsedRegs() const { return m_usedRegs; }

		bool isUsed(const JitReg& _reg) const;

		uint32_t pushSize(const JitReg& _reg);

		uint32_t pushedSize() const { return m_pushedBytes; }
		size_t pushedRegCount() const { return m_pushedRegs.size(); }

		void reset();

		void registerFuncArg(uint32_t _argIndex);
		void unregisterFuncArg(uint32_t _argIndex);

		bool isUsedFuncArg(const JitRegGP& _reg) const;

	private:
		void stackRegAdd(uint64_t _offset) const;
		void stackRegSub(uint64_t _offset) const;

		JitBlock& m_block;
		uint32_t m_pushedBytes = 0;
		uint32_t m_callCount = 0;

		struct PushedReg
		{
			uint32_t stackOffset = 0;
			JitReg reg;
			asmjit::BaseNode* cursorFirst = nullptr;
			asmjit::BaseNode* cursorLast = nullptr;

			bool operator < (const PushedReg& _r) const
			{
				return stackOffset > _r.stackOffset;	// reversed as stack is downwards
			}
		};

		std::vector<PushedReg> m_pushedRegs;
		std::vector<JitReg> m_usedRegs;
		std::set<uint32_t> m_usedFuncArgs;
	};
	
	class PushAllUsed
	{
	public:
		explicit PushAllUsed(JitBlock& _block, bool _begin = true);
		~PushAllUsed();

		void begin();
		void end();
	private:
		JitBlock& m_block;
		asmjit::BaseNode* m_cursorBeforePushes = nullptr;
	};
}
