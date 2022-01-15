#include "jitstackhelper.h"

#include "jitblock.h"
#include "jitemitter.h"

#include "dspassert.h"

#include "asmjit/core/builder.h"

#include <algorithm>	// std::sort

namespace dsp56k
{
	constexpr size_t g_stackAlignmentBytes = 16;
#ifdef HAVE_ARM64
	constexpr size_t g_functionCallSize = 0;
	constexpr auto g_stackReg = asmjit::a64::regs::sp;
#else
	constexpr size_t g_functionCallSize = 8;
	constexpr auto g_stackReg = asmjit::x86::rsp;
#endif
#ifdef _MSC_VER
	constexpr size_t g_shadowSpaceSize = 32;
#else
	constexpr size_t g_shadowSpaceSize = 0;
#endif

	constexpr bool g_dynamicNonVolatilePushes = true;

	JitStackHelper::JitStackHelper(JitBlock& _block) : m_block(_block)
	{
		m_pushedRegs.reserve(128);
		m_usedRegs.reserve(128);

		if constexpr (!g_dynamicNonVolatilePushes)
			pushNonVolatiles();
	}

	JitStackHelper::~JitStackHelper()
	{
		popAll();
	}

	void JitStackHelper::pushNonVolatiles()
	{
		for (const auto& reg : g_nonVolatileGPs)
			setUsed(reg);
		for (const auto& reg : g_nonVolatileXMMs)
		{
			if(reg.isValid())
				setUsed(reg);
		}
	}

	void JitStackHelper::push(const JitReg64& _reg)
	{
		PushedReg reg;
		reg.cursorFirst = m_block.asm_().cursor();
		m_block.asm_().push(_reg);
		reg.cursorFirst = reg.cursorFirst->next();
		reg.cursorLast = m_block.asm_().cursor();
		reg.reg = _reg;

		m_pushedBytes += pushSize(_reg);
		reg.stackOffset = m_pushedBytes;

		m_pushedRegs.push_back(reg);
	}

	void JitStackHelper::push(const JitReg128& _reg)
	{
		PushedReg reg;
		reg.cursorFirst = m_block.asm_().cursor();
		stackRegSub(pushSize(_reg));
		reg.cursorFirst = reg.cursorFirst->next();
		m_block.asm_().movq(ptr(g_stackReg), _reg);
		reg.cursorLast = m_block.asm_().cursor();
		reg.reg = _reg;

		m_pushedBytes += pushSize(_reg);
		reg.stackOffset = m_pushedBytes;

		m_pushedRegs.push_back(reg);
	}

	void JitStackHelper::pop(const JitReg64& _reg)
	{
		assert(!m_pushedRegs.empty());
		assert(m_pushedBytes >= pushSize(_reg));

		m_pushedRegs.pop_back();
		m_pushedBytes -= pushSize(_reg);

		m_block.asm_().pop(_reg);
	}

	void JitStackHelper::pop(const JitReg128& _reg)
	{
		assert(!m_pushedRegs.empty());
		assert(m_pushedBytes >= pushSize(_reg));

		m_pushedRegs.pop_back();
		m_pushedBytes -= pushSize(_reg);

		m_block.asm_().movq(_reg, ptr(g_stackReg));
		stackRegAdd(pushSize(_reg));
	}

	void JitStackHelper::pop(const JitReg& _reg)
	{
		if(_reg.isGp())
			pop(_reg.as<JitReg64>());
		else if(_reg.isVec())
			pop(_reg.as<JitReg128>());
		else
			assert(false && "unknown register type");
	}

	void JitStackHelper::pop()
	{
		const auto reg = m_pushedRegs.back();
		pop(reg.reg);
	}

	void JitStackHelper::popAll()
	{
		// we push stack-relative if there is at least one used vector register as we can save a bunch of instructions this way
		bool haveVectors = false;
		for (auto r : m_pushedRegs)
		{
			if(r.reg.isVec())
			{
				haveVectors = true;
				break;
			}
		}

		if(haveVectors)
		{
			// sort in order of memory address
			std::sort(m_pushedRegs.begin(), m_pushedRegs.end());

			stackRegAdd(m_pushedBytes);

			for(size_t i=0; i<m_pushedRegs.size(); ++i)
			{
				const auto& r = m_pushedRegs[i];
				const int offset = -static_cast<int>(m_pushedRegs[i].stackOffset);

				const auto memPtr = ptr(g_stackReg, offset);

				if(r.reg.isVec())
				{
					m_block.asm_().movq(r.reg.as<JitReg128>(), memPtr);
				}
				else
				{
#ifdef HAVE_ARM64
					m_block.asm_().ldr(r64(r.reg.as<JitRegGP>()), memPtr);
#else
					m_block.asm_().mov(r64(r.reg.as<JitRegGP>()), memPtr);
#endif
				}
			}

			m_pushedRegs.clear();
		}
		else
		{
			while (!m_pushedRegs.empty())
				pop();
		}
	}

	void JitStackHelper::call(const void* _funcAsPtr) const
	{
		PushBeforeFunctionCall backup(m_block);

		const auto usedSize = m_pushedBytes + g_functionCallSize;
		const auto alignedStack = (usedSize + g_stackAlignmentBytes-1) & ~(g_stackAlignmentBytes-1);

		const auto offset = alignedStack - usedSize + g_shadowSpaceSize;

		stackRegSub(offset);

		m_block.asm_().call(_funcAsPtr);

		stackRegAdd(offset);
	}

	void JitStackHelper::movePushesTo(asmjit::BaseNode* _baseNode, size_t _firstIndex)
	{
		m_block.asm_().setCursor(_baseNode->next());

		for(size_t i=_firstIndex; i<m_pushedRegs.size(); ++i)
		{
			auto* c = m_pushedRegs[i].cursorFirst;

			if (m_pushedRegs[i].reg.isVec())
				int foo = 0;
			while(true)
			{
				auto* next = c->next();
				auto* node = m_block.asm_().removeNode(c);
				m_block.asm_().addNode(node);
				if (c == m_pushedRegs[i].cursorLast)
					break;
				c = next;
			}
		}
		m_block.asm_().setCursor(m_block.asm_().lastNode());
	}

	bool JitStackHelper::isFuncArg(const JitRegGP& _gp)
	{
		for (const auto& gp : g_funcArgGPs)
		{
			if (gp.equals(r64(_gp)))
				return true;
		}
		return false;
	}

	bool JitStackHelper::isNonVolatile(const JitRegGP& _gp)
	{
		for (const auto& gp : g_nonVolatileGPs)
		{
			if(gp.equals(r64(_gp)))
				return true;
		}
		return false;
	}

	bool JitStackHelper::isNonVolatile(const JitReg128& _xm)
	{
		for (const auto& xm : g_nonVolatileXMMs)
		{
			if(xm.equals(_xm))
				return true;
		}
		return false;
	}

	void JitStackHelper::setUsed(const JitReg& _reg)
	{
		if(_reg.isGp())
			setUsed(_reg.as<JitRegGP>());
		else if(_reg.isVec())
			setUsed(_reg.as<JitReg128>());
		else
			assert(false && "unknown register type");
	}

	void JitStackHelper::setUsed(const JitRegGP& _reg)
	{
		if(isUsed(_reg))
			return;

		if(isNonVolatile(_reg))
			push(r64(_reg));

		m_usedRegs.push_back(_reg);
	}

	void JitStackHelper::setUsed(const JitReg128& _reg)
	{
		if(isUsed(_reg))
			return;

		if(isNonVolatile(_reg))
			push(_reg);

		m_usedRegs.push_back(_reg);
	}

	bool JitStackHelper::isUsed(const JitReg& _reg) const
	{
		for (const auto& m_usedReg : m_usedRegs)
		{
			if(m_usedReg.equals(_reg))
				return true;
		}
		return false;
	}

	uint32_t JitStackHelper::pushSize(const JitReg& _reg)
	{
#ifdef HAVE_ARM64
		return 16;
#else
		return _reg.size();
#endif
	}

	void JitStackHelper::stackRegAdd(uint64_t offset) const
	{
		if (!offset)
			return;

#ifdef HAVE_ARM64
		m_block.asm_().add(g_stackReg, g_stackReg, asmjit::Imm(offset));
#else
		m_block.asm_().add(g_stackReg, asmjit::Imm(offset));
#endif
	}

	void JitStackHelper::stackRegSub(uint64_t offset) const
	{
		if (!offset)
			return;

#ifdef HAVE_ARM64
		m_block.asm_().sub(g_stackReg, g_stackReg, asmjit::Imm(offset));
#else
		m_block.asm_().sub(g_stackReg, asmjit::Imm(offset));
#endif
	}
}
