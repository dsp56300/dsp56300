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
	}

	RegUsage::~RegUsage()
	{
	}

	void RegUsage::push(const JitReg& _reg)
	{
		m_pushedRegs.push_back(_reg);
		m_pushedBytes += _reg.size();

		m_block.asm_().push(_reg);
	}

	void RegUsage::pop(const JitReg& _reg)
	{
		assert(!m_pushedRegs.empty());
		assert(m_pushedBytes >= _reg.size());
		assert(m_pushedRegs.back().equals(_reg));

		m_pushedRegs.pop_back();
		m_pushedBytes -= _reg.size();

		m_block.asm_().pop(_reg);
	}

	void RegUsage::call(const void* _funcAsPtr)
	{
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

	void RegUsage::setUsed(const JitReg& _reg)
	{
		if(isNonVolatile(_reg))
			push(_reg);
	}
}
