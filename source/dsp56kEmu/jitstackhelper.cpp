#include "jitstackhelper.h"

#include "jitblock.h"
#include "jitemitter.h"

#include "dspassert.h"

namespace dsp56k
{
	constexpr size_t g_stackAlignmentBytes = 16;
#ifdef HAVE_ARM64
	constexpr size_t g_functionCallSize = 0;
#else
	constexpr size_t g_functionCallSize = 8;
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
	}

	void JitStackHelper::push(const JitRegGP& _reg)
	{
		m_block.asm_().push(_reg);

		m_pushedRegs.push_back(_reg);
		m_pushedBytes += pushSize(_reg);
	}

	void JitStackHelper::push(const JitReg128& _reg)
	{
		stackRegSub(pushSize(_reg));
		m_block.asm_().movq(ptr(asmjit::x86::rsp), _reg);

		m_pushedRegs.push_back(_reg);
		m_pushedBytes += pushSize(_reg);
	}

	void JitStackHelper::pop(const JitRegGP& _reg)
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

		m_block.asm_().movq(_reg, ptr(asmjit::x86::rsp));
		stackRegAdd(pushSize(_reg));
	}

	void JitStackHelper::pop(const JitReg& _reg)
	{
		if(_reg.isGp())
			pop(_reg.as<JitRegGP>());
		else if(_reg.isVec())
			pop(_reg.as<JitReg128>());
		else
			assert(false && "unknown register type");
	}

	void JitStackHelper::pop()
	{
		const auto reg = m_pushedRegs.back();
		pop(reg);
	}

	void JitStackHelper::popAll()
	{
		while(!m_pushedRegs.empty())
			pop();
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
			push(_reg);

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
		m_block.asm_().add(asmjit::a64::regs::sp, asmjit::a64::regs::sp, asmjit::Imm(offset));
#else
		m_block.asm_().add(asmjit::x86::rsp, asmjit::Imm(offset));
#endif
	}

	void JitStackHelper::stackRegSub(uint64_t offset) const
	{
		if (!offset)
			return;

#ifdef HAVE_ARM64
		m_block.asm_().sub(asmjit::a64::regs::sp, asmjit::a64::regs::sp, asmjit::Imm(offset));
#else
		m_block.asm_().sub(asmjit::x86::rsp, asmjit::Imm(offset));
#endif
	}
}
