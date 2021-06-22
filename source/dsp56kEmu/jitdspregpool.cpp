#include "jitdspregpool.h"

#include "dsp.h"
#include "jitblock.h"

#define LOGRP(S)		{}
//#define LOGRP(S)		LOG(S)

namespace dsp56k
{
	constexpr const char* g_dspRegNames[JitDspRegPool::DspCount] = 
	{
		"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",
		"n0",	"n1",	"n2",	"n3",	"n4",	"n5",	"n6",	"n7",
		"m0",	"m1",	"m2",	"m3",	"m4",	"m5",	"m6",	"m7",

		"a",	"b",
		"x",	"y",

		"extmem",
		"sr",
		"lc",
		"la"
	};

	JitDspRegPool::JitDspRegPool(JitBlock& _block) : m_block(_block)
	{
		clear();
	}

	JitDspRegPool::~JitDspRegPool()
	{
		storeAll();
	}

	JitReg JitDspRegPool::get(DspReg _reg, bool _read, bool _write)
	{
		const auto it = m_usedGpsMap.find(_reg);

		if(_write)
			m_writtenDspRegs.insert(_reg);

		if(it != m_usedGpsMap.end())
		{
			// The desired DSP register is already in a GP register. Nothing to be done except refresh the LRU list
			m_usedGps.remove(_reg);
			m_usedGps.push_back(_reg);
			LOGRP("DSP reg " << g_dspRegNames[_reg] << " already available, returning GP, now " << m_usedGps.size() << " used GPs");
			return it->second;
		}

		// No space left? Move some other GP reg to an XMM reg
		if(m_availableGps.empty())
		{
			LOGRP("No space left for DSP reg " << g_dspRegNames[_reg] << ", making space");
			makeSpace();
		}

		// There should be space left now
		assert(!m_availableGps.empty());

		// allocate a new slot for the GP register
		auto res = m_availableGps.front();
		m_availableGps.pop_front();
		m_usedGpsMap.insert(std::make_pair(_reg, res));
		m_usedGps.push_back(_reg);

		// Do we still have it in an XMM reg?
		const auto itXmm = m_usedXmmMap.find(_reg);
		if(itXmm != m_usedXmmMap.end())
		{
			LOGRP("DSP reg " <<g_dspRegNames[_reg] << " previously stored in xmm reg, restoring value");
			// yes, remove it and restore the content
			if(_read)
				m_block.asm_().movq(res, itXmm->second);
			m_usedXmmMap.erase(itXmm);
			m_usedXmms.remove(_reg);
		}
		else if(_read)
		{
			// no, load from memory
			LOGRP("Loading DSP reg " <<g_dspRegNames[_reg] << " from memory, now " << m_usedGps.size() << " GPs");
			load(res, _reg);
		}
		else
		{
			LOGRP("DSP reg " << g_dspRegNames[_reg] << " allocated, no read so no load, now " << m_usedGps.size() << " GPs");
		}

		return res;
	}

	void JitDspRegPool::read(const JitReg& _dst, const DspReg _src)
	{
		const auto r = get(_src, true, false);
		m_block.asm_().mov(_dst.r64(), r);
	}

	void JitDspRegPool::write(const DspReg _dst, const JitReg& _src)
	{
		const auto r = get(_dst, false, true);
		m_block.asm_().mov(r, _src.r64());
	}

	void JitDspRegPool::lock(DspReg _reg)
	{
		LOGRP("Locking DSP reg " <<g_dspRegNames[_reg]);
		const auto it = m_usedGpsMap.find(_reg);
		assert(it != m_usedGpsMap.end() && "unable to lock reg if not in use");
		assert(m_lockedGps.find(_reg) == m_lockedGps.end() && "register is already locked");
		m_lockedGps.insert(_reg);
	}

	void JitDspRegPool::unlock(DspReg _reg)
	{
		LOGRP("Unlocking DSP reg " <<g_dspRegNames[_reg]);
		const auto it = m_usedGpsMap.find(_reg);
		assert(it != m_usedGpsMap.end() && "unable to unlock reg if not in use");
		assert(m_lockedGps.find(_reg) != m_lockedGps.end() && "register is not locked");
		m_lockedGps.erase(_reg);
	}

	void JitDspRegPool::storeAll()
	{
		if(!m_writtenDspRegs.empty())
			LOGRP("Storing ALL written registers into memory");

		while(!m_writtenDspRegs.empty())
		{
			const auto r = *m_writtenDspRegs.begin();

			const auto itGp = m_usedGpsMap.find(r);
			if(itGp != m_usedGpsMap.end())
			{
				LOGRP("Storing modified DSP reg " << g_dspRegNames[r] << " from GP");
				store(r, itGp->second);
			}
			else
			{
				auto itXmm = m_usedXmmMap.find(r);
				assert(itXmm != m_usedXmmMap.end() && "XMM register not found for DSP register");
				LOGRP("Storing modified DSP reg " << g_dspRegNames[r] << " from XMM");
				store(r, itXmm->second);
			}
		}

		clear();
	}

	void JitDspRegPool::makeSpace()
	{
		// TODO: we can use upper bits of the XMMs, too
		if(m_availableXmms.empty())
		{
			const auto dspReg = m_usedXmms.front();

			LOGRP("No XMM temps left, writing DSP reg " <<g_dspRegNames[dspReg] << " back to memory");

			auto itXmm = m_usedXmmMap.find(dspReg);
			auto hostReg = itXmm->second;

			// TODO: discard a register that was NOT written first
			if(m_writtenDspRegs.find(dspReg) != m_writtenDspRegs.end())
				store(dspReg, hostReg);

			m_usedXmms.pop_front();
			m_usedXmmMap.erase(itXmm);
			m_availableXmms.push_back(hostReg);
		}

		// move the oldest used GP register to an XMM register
		for(auto it = m_usedGps.begin(); it != m_usedGps.end(); ++it)
		{
			const auto dspReg = *it;
			const auto hostReg = m_usedGpsMap.find(*it)->second;

			if(m_lockedGps.find(dspReg) != m_lockedGps.end())
				continue;

			LOGRP("Moving DSP reg " <<g_dspRegNames[dspReg] << " to XMM");

			m_usedGps.erase(it);
			m_usedGpsMap.erase(dspReg);

			const auto xmm = m_availableXmms.front();
			m_availableXmms.pop_front();
			m_block.asm_().movq(xmm, hostReg);
			m_usedXmmMap.insert(std::make_pair(dspReg, xmm));
			m_usedXmms.push_back(dspReg);

			m_availableGps.push_back(hostReg);
			return;
		}
		assert(false && "all GPs are locked, unable to make space");
	}

	void JitDspRegPool::clear()
	{
		m_availableGps.clear();
		m_availableXmms.clear();

		m_lockedGps.clear();

		m_usedGpsMap.clear();
		m_usedXmmMap.clear();
		m_usedXmms.clear();

		m_usedGps.clear();
		m_writtenDspRegs.clear();

		for(size_t i=0; i<m_gpCount; ++i)
			m_availableGps.push_back(m_gps[i]);

		for(size_t i=0; i<m_xmmCount; ++i)
			m_availableXmms.push_back(m_xmms[i]);
	}

	void JitDspRegPool::load(JitReg& _dst, const DspReg _src)
	{
		const auto& r = m_block.dsp().regs();
		auto& m = m_block.mem();

		switch (_src)
		{
		case DspR0:
		case DspR1:
		case DspR2:
		case DspR3:
		case DspR4:
		case DspR5:
		case DspR6:
		case DspR7:
			m.mov(_dst, r.r[_src - DspR0]);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			m.mov(_dst, r.n[_src - DspN0]);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			m.mov(_dst, r.m[_src - DspM0]);
			break;
		case DspA:
			m.mov(_dst, r.a);
			break;
		case DspB:
			m.mov(_dst, r.b);
			break;
		case DspX:
			m.mov(_dst, r.x);
			break;
		case DspY:
			m.mov(_dst, r.y);
			break;
		case DspExtMem:
			m.mov(_dst, m_block.dsp().memory().getBridgedMemoryAddress());
			break;
		case DspSR:
			m.mov(_dst, r.sr);
			break;
		case DspLC: 
			m.mov(_dst, r.lc);
			break;
		case DspLA: 
			m.mov(_dst, r.la);
			break;
		}
	}

	void JitDspRegPool::store(const DspReg _dst, JitReg& _src)
	{
		const auto& r = m_block.dsp().regs();
		auto& m = m_block.mem();

		switch (_dst)
		{
		case DspR0:
		case DspR1:
		case DspR2:
		case DspR3:
		case DspR4:
		case DspR5:
		case DspR6:
		case DspR7:
			m.mov(r.r[_dst - DspR0], _src);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			m.mov(r.n[_dst - DspN0], _src);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			m.mov(r.m[_dst - DspM0], _src);
			break;
		case DspA:
			m.mov(r.a, _src);
			break;
		case DspB:
			m.mov(r.b, _src);
			break;
		case DspX:
			m.mov(r.x, _src);
			break;
		case DspY:
			m.mov(r.y, _src);
			break;
		case DspExtMem:
			// read only
			break;
		case DspSR:
			m.mov(r.sr, _src);
			break;
		case DspLC: 
			m.mov(r.lc, _src);
			break;
		case DspLA: 
			m.mov(r.la, _src);
			break;
		}
		m_writtenDspRegs.erase(_dst);
	}

	void JitDspRegPool::store(const DspReg _dst, JitReg128& _src)
	{
		auto& r = m_block.dsp().regs();
		auto& m = m_block.mem();

		switch (_dst)
		{
		case DspR0:
		case DspR1:
		case DspR2:
		case DspR3:
		case DspR4:
		case DspR5:
		case DspR6:
		case DspR7:
			m.mov(r.r[_dst - DspR0], _src);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			m.mov(r.n[_dst - DspN0], _src);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			m.mov(r.m[_dst - DspM0], _src);
			break;
		case DspA:
			m.mov(r.a, _src);
			break;
		case DspB:
			m.mov(r.b, _src);
			break;
		case DspX:
			m.mov(r.x, _src);
			break;
		case DspY:
			m.mov(r.y, _src);
			break;
		case DspExtMem:
			// read only
			break;
		case DspSR:
			m.mov(r.sr, _src);
			break;
		case DspLC: 
			m.mov(r.lc, _src);
			break;
		case DspLA: 
			m.mov(r.la, _src);
			break;
		}
		m_writtenDspRegs.erase(_dst);
	}
}
