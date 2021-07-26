#include "jitdspregpool.h"

#include "dsp.h"
#include "jitblock.h"
#include "asmjit/x86/x86builder.h"

#define LOGRP(S)		{}
//#define LOGRP(S)		LOG(S)

namespace dsp56k
{
	static constexpr uint32_t g_gpCount = sizeof(g_dspPoolGps) / sizeof(g_dspPoolGps[0]);
	static constexpr uint32_t g_xmmCount = sizeof(g_dspPoolXmms) / sizeof(g_dspPoolXmms[0]);

	constexpr const char* g_dspRegNames[] = 
	{
		"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",
		"n0",	"n1",	"n2",	"n3",	"n4",	"n5",	"n6",	"n7",
		"m0",	"m1",	"m2",	"m3",	"m4",	"m5",	"m6",	"m7",

		"ar",	"br",
		"aw",	"bw",

		"x",	"y",

		"extmem",
		"sr",
		"lc",
		"la",

		"ta", "tb", "tc", "td", "te", "tf", "tg", "th"
	};

	static_assert((sizeof(g_dspRegNames) / sizeof(g_dspRegNames[0])) == JitDspRegPool::DspCount);

	JitDspRegPool::JitDspRegPool(JitBlock& _block) : m_block(_block), m_lockedGps(0), m_writtenDspRegs(0)
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

	JitRegGP JitDspRegPool::get(DspReg _reg, bool _read, bool _write)
	{
		if(_write)
		{
			setWritten(_reg);

			if(!_read)
			{
				// If a previous parallel op wrote to the same register, discard what was written
				switch (_reg)
				{
				case DspA:
					if(isInUse(DspAwrite))
					{
						clearWritten(DspAwrite);
						release(DspAwrite, false);
					}
					break;
				case DspB:
					if(isInUse(DspBwrite))
					{
						clearWritten(DspBwrite);
						release(DspBwrite, false);					
					}
					break;
				}

				m_dspPtr.reset();
			}
		}

		if(m_gpList.isUsed(_reg))
		{
			// The desired DSP register is already in a GP register. Nothing to be done except refresh the LRU list

			JitRegGP res;
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
		JitRegGP res;
		m_gpList.acquire(res, _reg, m_repMode);
		m_block.stack().setUsed(res);

		// Do we still have it in an XMM reg?
		JitReg128 xmReg;
		if(m_xmList.release(xmReg, _reg, m_repMode))
		{
			// yes, remove it and restore the content
			LOGRP("DSP reg " <<g_dspRegNames[_reg] << " previously stored in xmm reg, restoring value");
			if(_read)
				m_block.asm_().movq(res, xmReg);
		}
		else if(_read)
		{
			// no, load from memory
			LOGRP("Loading DSP reg " <<g_dspRegNames[_reg] << " from memory, now " << m_gpList.size() << " GPs");
			load(res, _reg);
		}
		else
		{
			LOGRP("DSP reg " << g_dspRegNames[_reg] << " allocated, no read so no load, now " << m_gpList.size() << " GPs");
		}

		return res;
	}

	void JitDspRegPool::read(const JitRegGP& _dst, const DspReg _src)
	{
		const auto r = get(_src, true, false);
		m_block.asm_().mov(_dst.r64(), r);
	}

	void JitDspRegPool::write(const DspReg _dst, const JitRegGP& _src)
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
			release(static_cast<DspReg>(i), false);

		m_dspPtr.reset();

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
				release(r, false);
		}
		m_dspPtr.reset();
	}

	bool JitDspRegPool::isInUse(const JitReg128& _xmm) const
	{
		return m_xmList.isUsed(_xmm);
	}

	bool JitDspRegPool::isInUse(const JitRegGP& _gp) const
	{
		return m_gpList.isUsed(_gp);
	}

	bool JitDspRegPool::isInUse(DspReg _reg) const
	{
		return m_gpList.isUsed(_reg) || m_xmList.isUsed(_reg);
	}

	JitDspRegPool::DspReg JitDspRegPool::aquireTemp()
	{
		assert(!m_availableTemps.empty());
		const auto res = m_availableTemps.back();
		m_availableTemps.pop_back();
		return res;
	}

	void JitDspRegPool::releaseTemp(DspReg _reg)
	{
		m_availableTemps.push_back(_reg);
		release(_reg);
	}
	
	bool JitDspRegPool::move(const JitRegGP& _dst, const DspReg _src)
	{
		JitRegGP gpSrc;
		JitReg128 xmSrc;

		if(m_gpList.get(gpSrc, _src))
			m_block.asm_().mov(_dst, gpSrc);
		else if(m_xmList.get(xmSrc, _src))
			m_block.asm_().movq(_dst, xmSrc);
		else
			return false;
		return true;
	}

	void JitDspRegPool::setIsParallelOp(bool _isParallelOp)
	{
		m_isParallelOp = _isParallelOp;
	}

	bool JitDspRegPool::move(DspReg _dst, DspReg _src)
	{
		JitRegGP gpSrc;
		JitReg128 xmSrc;

		if(m_gpList.get(gpSrc, _src))
		{
			// src is GP

			JitRegGP gpDst;
			JitReg128 xmDst;

			if(m_gpList.get(gpDst, _dst))
				m_block.asm_().mov(gpDst, gpSrc);
			else if(m_xmList.get(xmDst, _dst))
				m_block.asm_().movq(xmDst, gpSrc);
			else
				return false;
			return true;
		}

		if(m_xmList.get(xmSrc, _src))
		{
			// src is XMM

			JitRegGP gpDst;
			JitReg128 xmDst;

			if(m_gpList.get(gpDst, _dst))
				m_block.asm_().movq(gpDst, xmSrc);
			else if(m_xmList.get(xmDst, _dst))
				m_block.asm_().movq(xmDst, xmSrc);
			else
				return false;
			return true;
		}
		return false;
	}

	void JitDspRegPool::parallelOpEpilog()
	{
		parallelOpEpilog(DspA, DspAwrite);
		parallelOpEpilog(DspB, DspBwrite);
	}

	void JitDspRegPool::parallelOpEpilog(const DspReg _aluReadReg, const DspReg _aluWriteReg)
	{
		if(!isLocked(_aluWriteReg))
			return;

		unlock(_aluWriteReg);

		if(!isInUse(_aluReadReg))
		{
			get(_aluReadReg, true, true);
		}
		else
		{
			move(_aluReadReg, _aluWriteReg);
			setWritten(_aluReadReg);
		}

		clearWritten(_aluWriteReg);
		release(_aluWriteReg);
	}

	void JitDspRegPool::makeSpace(const DspReg _wantedReg)
	{
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

			JitRegGP hostReg;
			m_gpList.release(hostReg, dspReg, m_repMode);

			JitReg128 xmReg;
			m_xmList.acquire(xmReg, dspReg, m_repMode);
			m_block.stack().setUsed(xmReg);

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
			m_gpList.addHostReg(g_dspPoolGps[i]);

		for(size_t i=0; i<g_xmmCount; ++i)
			m_xmList.addHostReg(g_dspPoolXmms[i]);

		m_availableTemps.clear();

		for(int i=TempA; i<=LastTemp; ++i)
			m_availableTemps.push_back(static_cast<DspReg>(i));
	}

	void JitDspRegPool::load(JitRegGP& _dst, const DspReg _src)
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
			if(!isLocked(DspAwrite) && isInUse(DspAwrite))
			{
				move(_dst, DspAwrite);
				if(!m_isParallelOp)
				{
					clearWritten(DspAwrite);
					release(DspAwrite);
					setWritten(DspA);
				}
			}
			else
				m.mov(_dst, r.a);
			break;
		case DspAwrite:
			// write only
			break;
		case DspB:
			if(!isLocked(DspBwrite) && isInUse(DspBwrite))
			{
				move(_dst, DspBwrite);
				if(!m_isParallelOp)
				{
					clearWritten(DspBwrite);
					release(DspBwrite);
					setWritten(DspB);					
				}
			}
			else
				m.mov(_dst, r.b);
			break;
		case DspBwrite:
			// write only
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

	void JitDspRegPool::store(const DspReg _dst, JitRegGP& _src, bool _resetBasePtr/* = true*/)
	{
		const auto& r = m_block.dsp().regs();

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
			mov(r.r[_dst - DspR0], _src);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			mov(r.n[_dst - DspN0], _src);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			mov(r.m[_dst - DspM0], _src);
			break;
		case DspA:
		case DspAwrite:
			mov(r.a, _src);
			break;
		case DspB:
		case DspBwrite:
			mov(r.b, _src);
			break;
		case DspX:
			mov(r.x, _src);
			break;
		case DspY:
			mov(r.y, _src);
			break;
		case DspExtMem:
			// read only
			break;
		case DspSR:
			mov(r.sr, _src);
			break;
		case DspLC: 
			mov(r.lc, _src);
			break;
		case DspLA: 
			mov(r.la, _src);
			break;
		}

		if(_resetBasePtr)
			m_dspPtr.reset();
	}

	void JitDspRegPool::store(const DspReg _dst, JitReg128& _src, bool _resetBasePtr/* = true*/)
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
			mov(r.r[_dst - DspR0], _src);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			mov(r.n[_dst - DspN0], _src);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			mov(r.m[_dst - DspM0], _src);
			break;
		case DspA:
		case DspAwrite:
			mov(r.a, _src);
			break;
		case DspB:
		case DspBwrite:
			mov(r.b, _src);
			break;
		case DspX:
			mov(r.x, _src);
			break;
		case DspY:
			mov(r.y, _src);
			break;
		case DspExtMem:
			// read only
			break;
		case DspSR:
			mov(r.sr, _src);
			break;
		case DspLC: 
			mov(r.lc, _src);
			break;
		case DspLA: 
			mov(r.la, _src);
			break;
		}

		if(_resetBasePtr)
			m_dspPtr.reset();
	}

	bool JitDspRegPool::release(DspReg _dst, bool _resetBasePtr)
	{
		if(isLocked(_dst))
		{
			LOGRP("Unable to release DSP reg " << g_dspRegNames[_dst] << " as its locked");
			return false;
		}

		JitRegGP gpReg;
		if(m_gpList.release(gpReg, _dst, m_repMode))
		{
			if(isWritten(_dst))
			{
				LOGRP("Storing modified DSP reg " << g_dspRegNames[_dst] << " from GP");
				store(_dst, gpReg, _resetBasePtr);
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
				store(_dst, xmReg, _resetBasePtr);
				clearWritten(_dst);
			}						
		}

		return true;
	}

	asmjit::x86::Mem JitDspRegPool::makeDspPtr(const void* _ptr, const size_t _size)
	{
		const void* base = &m_block.dsp().regs();
		const auto offset = static_cast<const uint8_t*>(_ptr) - static_cast<const uint8_t*>(base);
		assert(offset < 0xffffffff);
		
		if(!m_dspPtr.hasBase() || !m_dspPtr.hasSize())
		{
			m_block.asm_().mov(regReturnVal, asmjit::Imm(reinterpret_cast<uint64_t>(base)));
			m_dspPtr = ptr(regReturnVal, 0, static_cast<uint32_t>(_size));
		}

		m_dspPtr.setSize(static_cast<uint32_t>(_size));
		m_dspPtr.setOffset(offset);

		return m_dspPtr;
	}

	void JitDspRegPool::mov(const asmjit::x86::Mem& _dst, const JitRegGP& _src) const
	{
		m_block.asm_().mov(_dst, _src);
	}

	void JitDspRegPool::movd(const asmjit::x86::Mem& _dst, const JitReg128& _src) const
	{
		m_block.asm_().movd(_dst, _src);
	}

	void JitDspRegPool::movq(const asmjit::x86::Mem& _dst, const JitReg128& _src) const
	{
		m_block.asm_().movq(_dst, _src);
	}
}
