#include "jitregusage.h"

#include <cassert>

#include "jitblock.h"

namespace dsp56k
{
	constexpr size_t g_nonVolatileGPCount  = sizeof(g_nonVolatileGPs)  / sizeof(g_nonVolatileGPs[0]);
	constexpr size_t g_nonVolatileXMMCount = sizeof(g_nonVolatileXMMs) / sizeof(g_nonVolatileXMMs[0]);

	constexpr size_t g_stackAlignmentBytes = 16;
	constexpr size_t g_functionCallSize = 8;
	
	RegUsage::RegUsage(JitBlock& _block) : m_block(_block)
	{
		m_pushedRegs.reserve(128);
		m_usedRegs.reserve(128);

		for (const auto& reg : g_nonVolatileGPs)
			push(reg);
	}

	RegUsage::~RegUsage()
	{
		popAll();
	}

	void RegUsage::push(const JitReg& _reg)
	{
		m_block.asm_().push(_reg);

		m_pushedRegs.push_back(_reg);
		m_pushedBytes += _reg.size();
	}

	void RegUsage::push(const JitReg128& _reg)
	{
		m_block.asm_().movq(regReturnVal, _reg);
		m_block.asm_().push(regReturnVal);

		m_pushedRegs.push_back(_reg);
		m_pushedBytes += regReturnVal.size();
	}

	void RegUsage::pop(const JitReg& _reg)
	{
		assert(!m_pushedRegs.empty());
		assert(m_pushedBytes >= _reg.size());

		m_pushedRegs.pop_back();
		m_pushedBytes -= _reg.size();

		m_block.asm_().pop(_reg);
	}

	void RegUsage::pop(const JitReg128& _reg)
	{
		pop(regReturnVal);
		m_block.asm_().movq(_reg, regReturnVal);
	}

	void RegUsage::pop(const Reg& _reg)
	{
		if(_reg.isGp())
			pop(_reg.as<JitReg>());
		else if(_reg.isVec())
			pop(_reg.as<JitReg128>());
		else
			assert(false && "unknown register type");
	}

	void RegUsage::pop()
	{
		auto reg = m_pushedRegs.back();
		pop(reg);
	}

	void RegUsage::popAll()
	{
		while(!m_pushedRegs.empty())
			pop();
	}

	void RegUsage::call(const void* _funcAsPtr)
	{
		PushBeforeFunctionCall backup(m_block);
		m_block.asm_().call(_funcAsPtr);
	}

	bool RegUsage::isNonVolatile(const JitReg& _gp)
	{
		for(size_t i=0; i<g_nonVolatileGPCount; ++i)
		{
			if(g_nonVolatileGPs[i].equals(_gp))
				return true;
		}
		return false;
	}

	bool RegUsage::isNonVolatile(const JitReg128& _xm)
	{
		for(size_t i=0; i<g_nonVolatileXMMCount; ++i)
		{
			if(g_nonVolatileXMMs[i].equals(_xm))
				return true;
		}
		return false;
	}

	void RegUsage::setUsed(const Reg& _reg)
	{
		if(_reg.isGp())
			setUsed(_reg.as<JitReg>());
		else if(_reg.isVec())
			setUsed(_reg.as<JitReg128>());
		else
			assert(false && "unknown register type");
	}

	void RegUsage::setUsed(const JitReg& _reg)
	{
		if(isUsed(_reg))
			return;

//		if(isNonVolatile(_reg))
//			push(_reg);

		m_usedRegs.push_back(_reg);
	}

	void RegUsage::setUsed(const JitReg128& _reg)
	{
		if(isUsed(_reg))
			return;

//		if(isNonVolatile(_reg))
//			push(_reg);

		m_usedRegs.push_back(_reg);
	}

	bool RegUsage::isUsed(const Reg& _reg) const
	{
		for (const auto& m_usedReg : m_usedRegs)
		{
			if(m_usedReg.equals(_reg))
				return true;
		}
		return false;
	}
}
