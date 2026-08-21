#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <vector>

#include "jittypes.h"

namespace asmjit
{
	inline namespace ASMJIT_ABI_NAMESPACE
	{
		class BaseNode;
	}
}

namespace dsp56k
{
	class JitBlockRuntimeData;
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
		
		void pushAllUsed(asmjit::BaseNode* _baseNode);

		void call(const std::function<void()>& _execCall);
		void call(const void* _funcAsPtr);

		static bool isFuncArg(const JitRegGP& _gp, uint32_t _maxIndex = 255);
		static bool isNonVolatile(const JitReg& _gp);
		static bool isNonVolatile(const JitRegGP& _gp);
		static bool isNonVolatile(const JitReg128& _xm);

		// isNonVolatile() answers the ABI question "does a C++ callee preserve this for me", which is what
		// decides whether we have to push around a call. blockMustPreserve() answers the different question
		// "does this block have to save it for its caller" - false for everything the trampoline saves once
		// per batch. Mixing the two costs far more than it saves: a prolog push happens once per block entry,
		// a push around a call happens every time the block calls into C++.
		static bool blockMustPreserve(const JitReg& _gp);
		static bool blockMustPreserve(const JitRegGP& _gp);
		static bool blockMustPreserve(const JitReg128& _xm);

		void setUsed(const JitReg& _reg);
		void setUsed(const JitRegGP& _reg);
		void setUsed(const JitReg128& _reg);

		void setUnused(const JitReg& _reg);

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
