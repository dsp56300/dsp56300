#include "jitdspregpool.h"

#include "dsp.h"
#include "jitblock.h"

#define LOGRP(S)		{}
//#define LOGRP(S)		LOG(S)

namespace dsp56k
{
	static constexpr JitReg g_gps[] =	{ asmjit::x86::r8, asmjit::x86::r9, asmjit::x86::r10, asmjit::x86::r11};

	static constexpr JitReg128 g_xmms[] =	{ asmjit::x86::xmm6, asmjit::x86::xmm7, asmjit::x86::xmm8, asmjit::x86::xmm9, asmjit::x86::xmm10, asmjit::x86::xmm11
											, asmjit::x86::xmm0, asmjit::x86::xmm1, asmjit::x86::xmm2, asmjit::x86::xmm3, asmjit::x86::xmm4, asmjit::x86::xmm5};

	static constexpr uint32_t g_gpCount = sizeof(g_gps) / sizeof(g_gps[0]);
	static constexpr uint32_t g_xmmCount = sizeof(g_xmms) / sizeof(g_xmms[0]);

	constexpr const char* g_dspRegNames[] = 
	{
		"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",
		"n0",	"n1",	"n2",	"n3",	"n4",	"n5",	"n6",	"n7",
		"m0",	"m1",	"m2",	"m3",	"m4",	"m5",	"m6",	"m7",

		"a",	"b",
		"x",	"y",

		"extmem",
		"sr",
		"lc",
		"la",

		"ta", "tb", "tc", "td", "te", "tf", "tg", "th"
	};

	static_assert((sizeof(g_dspRegNames) / sizeof(g_dspRegNames[0])) == JitDspRegPool::DspCount);

	JitDspRegPool::JitDspRegPool(JitBlock& _block) : m_block(_block)
	{
		clear();
	}

	JitDspRegPool::~JitDspRegPool()
	{
		releaseWritten();
/*		releaseAll();

		assert(m_availableGps.size() == (m_gpCount - m_lockedGps.size()));
		assert(m_availableXmms.size() == m_xmmCount);
		assert(m_writtenDspRegs.size() <= m_lockedGps.size());
		assert(m_usedGps.size() == m_lockedGps.size());
		assert(m_usedGpsMap.size() == m_lockedGps.size());
		assert(m_usedXmms.empty());
		assert(m_usedXmmMap.empty());		
*/	}

	JitReg JitDspRegPool::get(DspReg _reg, bool _read, bool _write)
	{
		if(_write)
			setWritten(_reg);

		if(m_gpList.isUsed(_reg))
		{
			// The desired DSP register is already in a GP register. Nothing to be done except refresh the LRU list

			JitReg res;
			m_gpList.acquire(res, _reg, m_repMode);

			LOGRP("DSP reg " << g_dspRegNames[_reg] << " already available, returning GP, " << m_gpList.size() << " used GPs");
			return res;
		}

		// No space left? Move some other GP reg to an XMM reg
		if(m_gpList.isFull())
		{
			LOGRP("No space left for DSP reg " << g_dspRegNames[_reg] << ", making space");
			makeSpace(_reg);
		}

		// There should be space left now
		assert(!m_gpList.isFull());

		// allocate a new slot for the GP register
		JitReg res;
		m_gpList.acquire(res, _reg, m_repMode);

		// Do we still have it in an XMM reg?
		JitReg128 xmReg;
		if(m_xmList.release(xmReg, _reg, m_repMode))
		{
			// yes, remove it and restore the content
			LOGRP("DSP reg " <<g_dspRegNames[_reg] << " previously stored in xmm reg, restoring value");
//			if(_read)
				m_block.asm_().movq(res, xmReg);
		}
		else// if(_read)
		{
			// no, load from memory
			LOGRP("Loading DSP reg " <<g_dspRegNames[_reg] << " from memory, now " << m_gpList.size() << " GPs");
			load(res, _reg);
		}
/*		else
		{
			LOGRP("DSP reg " << g_dspRegNames[_reg] << " allocated, no read so no load, now " << m_gpList.size() << " GPs");
		}
*/
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
		assert(m_gpList.isUsed(_reg) && "unable to lock reg if not in use");
		assert(!isLocked(_reg) && "register is already locked");
		setLocked(_reg);
	}

	void JitDspRegPool::unlock(DspReg _reg)
	{
		LOGRP("Unlocking DSP reg " <<g_dspRegNames[_reg]);
		assert(m_gpList.isUsed(_reg) && "unable to unlock reg if not in use");
		assert(isLocked(_reg) && "register is not locked");
		clearLocked(_reg);
	}

	void JitDspRegPool::releaseAll()
	{
		for(size_t i=0; i<DspCount; ++i)
			release(static_cast<DspReg>(i));

		assert(m_gpList.empty());
		assert(m_xmList.empty());
		assert(m_writtenDspRegs == 0);
		assert(m_lockedGps == 0);
		assert(m_gpList.available() == g_gpCount);
		assert(m_xmList.available() == g_xmmCount);

		// We use this to restore ordering of GPs and XMMs as they need to be predictable in native loops
		clear();
	}

	void JitDspRegPool::releaseWritten()
	{
		if(m_writtenDspRegs == 0)
			return;
		
		LOGRP("Storing ALL written registers into memory");

		for(size_t i=0; i<DspCount; ++i)
		{
			const auto r = static_cast<DspReg>(i);
			if(isWritten(r))
				release(r);
		}
	}

	bool JitDspRegPool::isInUse(const JitReg128& _xmm) const
	{
		return m_xmList.isUsed(_xmm);
	}

	bool JitDspRegPool::isInUse(const JitReg& _gp) const
	{
		return m_gpList.isUsed(_gp);
	}

	JitDspRegPool::DspReg JitDspRegPool::aquireTemp()
	{
		assert(!m_availableTemps.empty());
		const auto res = m_availableTemps.front();
		m_availableTemps.pop_front();
		return res;
	}

	void JitDspRegPool::releaseTemp(DspReg _reg)
	{
		push(m_availableTemps, _reg);
		release(_reg);
	}

	void JitDspRegPool::makeSpace(DspReg _wantedReg)
	{
		// TODO: we can use upper bits of the XMMs, too
		if(m_xmList.isFull())
		{
			LOGRP("No XMM temps left, writing a DSP reg back to memory");

			for(auto it = m_xmList.used().begin(); it != m_xmList.used().end(); ++it)
			{
				const auto dspReg = *it;
				
				if(dspReg == _wantedReg)
					continue;

				LOGRP("Writing DSP reg " <<g_dspRegNames[dspReg] << " back to memory to make space");

				// TODO: discard a register that was NOT written first
				
				const auto res = release(dspReg);
				assert(res && "unable to release XMM reg");
				break;
			}
		}

		// move the oldest used GP register to an XMM register
		for(auto it = m_gpList.used().begin(); it != m_gpList.used().end(); ++it)
		{
			const DspReg dspReg = *it;

			if(isLocked(dspReg))
				continue;

			if(dspReg == _wantedReg)
				continue;

			LOGRP("Moving DSP reg " <<g_dspRegNames[dspReg] << " to XMM");

			JitReg hostReg;
			m_gpList.release(hostReg, dspReg, m_repMode);

			JitReg128 xmReg;
			m_xmList.acquire(xmReg, dspReg, m_repMode);

			m_block.asm_().movq(xmReg, hostReg);

			return;
		}
		assert(false && "all GPs are locked, unable to make space");
	}

	void JitDspRegPool::clear()
	{
		m_gpList.clear();
		m_xmList.clear();

		m_lockedGps = 0;
		m_writtenDspRegs = 0;

		for(size_t i=0; i<g_gpCount; ++i)
			m_gpList.addHostReg(g_gps[i]);

		for(size_t i=0; i<g_xmmCount; ++i)
			m_xmList.addHostReg(g_xmms[i]);

		m_availableTemps.clear();

		for(int i=TempA; i<=LastTemp; ++i)
			m_availableTemps.push_back(static_cast<DspReg>(i));
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
	}

	bool JitDspRegPool::release(DspReg _dst)
	{
		if(isLocked(_dst))
		{
			LOGRP("Unable to release DSP reg " << g_dspRegNames[_dst] << " as its locked");
			return false;
		}

		JitReg gpReg;
		if(m_gpList.release(gpReg, _dst, m_repMode))
		{
			if(isWritten(_dst))
			{
				LOGRP("Storing modified DSP reg " << g_dspRegNames[_dst] << " from GP");
				store(_dst, gpReg);
				clearWritten(_dst);
			}
			return true;
		}
		
		JitReg128 xmReg;
		if(m_xmList.release(xmReg, _dst, m_repMode))
		{
			if(isWritten(_dst))
			{
				LOGRP("Storing modified DSP reg " << g_dspRegNames[_dst] << " from XMM");
				store(_dst, xmReg);
				clearWritten(_dst);
			}						
		}

		return true;
	}
}
