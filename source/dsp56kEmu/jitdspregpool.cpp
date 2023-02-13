#include "jitdspregpool.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitemitter.h"

#include <limits>

#define LOGRP(S)		{}
//#define LOGRP(S)		LOG(S)

namespace dsp56k
{
	constexpr const char* g_dspRegNames[] = 
	{
		"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",
		"n0",	"n1",	"n2",	"n3",	"n4",	"n5",	"n6",	"n7",
		"m0",	"m1",	"m2",	"m3",	"m4",	"m5",	"m6",	"m7",

		"ar",	"br",
		"aw",	"bw",

		"x",	"y",

		"pc",
		"sr",
		"lc",
		"la",

		"ta", "tb", "tc", "td", "te", "tf", "tg", "th",

		"m0mod",	"m1mod",	"m2mod",	"m3mod",	"m4mod",	"m5mod",	"m6mod",	"m7mod",
		"m0mask",	"m1mask",	"m2mask",	"m3mask",	"m4mask",	"m5mask",	"m6mask",	"m7mask",
	};

	static_assert(std::size(g_dspRegNames) == JitDspRegPool::DspCount);
	static_assert(JitDspRegPool::DspCount < 64);	// our flag sets for used/read/written registers is 64 bits wide

	JitDspRegPool::JitDspRegPool(JitBlock& _block) : m_block(_block), m_extendedSpillSpace(false)//_block.asm_().hasFeature(asmjit::CpuFeatures::X86::kSSE4_1))
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

	JitRegGP JitDspRegPool::get(const JitRegGP& _dst, const DspReg _reg, bool _read, bool _write)
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
						release(DspAwrite);
					}
					break;
				case DspB:
					if(isInUse(DspBwrite))
					{
						clearWritten(DspBwrite);
						release(DspBwrite);					
					}
					break;
				}
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

		if(_dst.isValid() && _read && !_write)
		{
			SpillReg xm;
			if(m_xmList.get(xm, _reg))
			{
				LOGRP("DSP reg " << g_dspRegNames[_reg] << " available as xmm but not GP, copying to external GP directly");
				spillMove(_dst, xm);
				m_moveToXmmInstruction[_reg] = nullptr;
				return _dst;
			}
		}

		m_dirty = true;

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
		SpillReg xmReg;
		if(m_xmList.release(xmReg, _reg, m_repMode))
		{
			// yes, remove it and restore the content
			LOGRP("DSP reg " << g_dspRegNames[_reg] << " previously stored in xmm reg, restoring value");
			if(_read)
				spillMove(res, xmReg);

			clearSpilled(_reg);

			m_moveToXmmInstruction[_reg] = nullptr;
		}
		else if(_read)
		{
			// no, load from memory
			LOGRP("Loading DSP reg " << g_dspRegNames[_reg] << " from memory, now " << m_gpList.size() << " GPs");
			load(res, _reg);
		}
		else
		{
			LOGRP("DSP reg " << g_dspRegNames[_reg] << " allocated, no read so no load, now " << m_gpList.size() << " GPs");
		}

		return res;
	}

	DspValue JitDspRegPool::read(const DspReg _src) const
	{
		return DspValue(m_block, _src, true, false);
	}

	void JitDspRegPool::read(const JitRegGP& _dst, const DspReg _src)
	{
		const auto r = get(_dst, _src, true, false);
		if(r != _dst)
			m_block.asm_().mov(r64(_dst), r);
	}

	void JitDspRegPool::write(const DspReg _dst, const JitRegGP& _src)
	{
		const auto r = get(_dst, false, true);
		m_block.asm_().mov(r, r64(_src));
	}

	void JitDspRegPool::write(const DspReg _dst, const DspValue& _src)
	{
		if (_src.isDspReg(_dst))
		{
			const auto reg = get(_dst, _src.getDspReg().read(), true);
			assert(r64(reg) == r64(_src));
		}
		else
		{
			_src.copyTo(get(_dst, false, true), DspValue::getBitCount(_dst));
		}
	}

	void JitDspRegPool::lock(const DspReg _reg)
	{
		LOGRP("Locking DSP reg " <<g_dspRegNames[_reg]);
		assert(m_gpList.isUsed(_reg) && "unable to lock reg if not in use");
		assert(!isLocked(_reg) && "register is already locked");
		setLocked(_reg);
	}

	void JitDspRegPool::unlock(const DspReg _reg)
	{
		LOGRP("Unlocking DSP reg " <<g_dspRegNames[_reg]);
		assert(m_gpList.isUsed(_reg) && "unable to unlock reg if not in use");
		assert(isLocked(_reg) && "register is not locked");
		clearLocked(_reg);
	}

	void JitDspRegPool::releaseNonLocked()
	{
		for (size_t i = 0; i < DspCount; ++i)
		{
			const auto r = static_cast<DspReg>(i);
			if(!isLocked(r))
				release(r);
		}
	}

	void JitDspRegPool::releaseAll()
	{
		for(size_t i=0; i<DspCount; ++i)
			release(static_cast<DspReg>(i));

		assert(m_gpList.empty());
		assert(m_xmList.empty());

		assert(!hasWrittenRegs());
		assert(m_lockedGps == DspRegFlags::None);
		assert(m_loadedDspRegs == DspRegFlags::None);
		assert(m_loadedDspRegs == DspRegFlags::None);
		assert(m_spilledDspRegs == DspRegFlags::None);

		assert(m_gpList.available() == std::size(g_dspPoolGps));
		assert(m_xmList.available() == (m_extendedSpillSpace ? std::size(g_dspPoolXmms) * 2 : std::size(g_dspPoolXmms)));

		// We use this to restore ordering of GPs and XMMs as they need to be predictable in native loops
		clear();
	}

	void JitDspRegPool::releaseWritten()
	{
		if(!hasWrittenRegs())
			return;

		LOGRP("Storing ALL written registers into memory");

		releaseByFlags(m_writtenDspRegs);
	}

	void JitDspRegPool::releaseLoaded()
	{
		releaseByFlags(m_loadedDspRegs);
	}

	void JitDspRegPool::releaseByFlags(const DspRegFlags _flags)
	{
		if(_flags == DspRegFlags::None)
			return;

		for(size_t i=0; i<DspCount; ++i)
		{
			const auto r = static_cast<DspReg>(i);
			if(flagTest(_flags, r))
				release(r);
		}
	}

	void JitDspRegPool::debugStoreAll()
	{
		for(auto i=0; i<DspCount; ++i)
		{
			const auto r = static_cast<DspReg>(i);

			JitRegGP gp;
			SpillReg xm;

			if(m_gpList.get(gp, r))
				store(r, gp);
			else if(m_xmList.get(xm, r))
				store(r, xm);
		}
	}

	bool JitDspRegPool::isInUse(const JitReg128& _xmm) const
	{
		return m_xmList.isUsed({_xmm, 0}) || m_xmList.isUsed({_xmm, 1});
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
		SpillReg xmSrc;

		if(m_gpList.get(gpSrc, _src))
			m_block.asm_().mov(_dst, gpSrc);
		else if(m_xmList.get(xmSrc, _src))
		{
			spillMove(_dst, xmSrc);
			m_moveToXmmInstruction[_src] = nullptr;
		}
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
		SpillReg xmSrc;

		if(m_gpList.get(gpSrc, _src))
		{
			// src is GP

			JitRegGP gpDst;
			SpillReg xmDst;

			if(m_gpList.get(gpDst, _dst))
				m_block.asm_().mov(gpDst, gpSrc);
			else if(m_xmList.get(xmDst, _dst))
			{
				spillMove(xmDst, gpSrc);
				m_moveToXmmInstruction[_dst] = m_block.asm_().cursor();
			}
			else
				return false;
			return true;
		}

		if(m_xmList.get(xmSrc, _src))
		{
			// src is XMM

			JitRegGP gpDst;
			SpillReg xmDst;

			if(m_gpList.get(gpDst, _dst))
			{
				spillMove(gpDst, xmSrc);
				m_moveToXmmInstruction[_dst] = nullptr;
			}
			else if(m_xmList.get(xmDst, _dst))
			{
				spillMove(xmDst, xmSrc);
				m_moveToXmmInstruction[_dst] = m_block.asm_().cursor();
			}
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

	void JitDspRegPool::movDspReg(const TReg5& _dst, const DspValue& _src) const
	{
		m_block.mem().mov(makeDspPtr(_dst), _src);
	}

	void JitDspRegPool::movDspReg(DspValue& _dst, const TReg5& _src) const
	{
		if (!_dst.isRegValid() || _dst.getBitCount() != 8)
			_dst.temp(DspValue::Temp8);
		m_block.mem().mov(_dst, makeDspPtr(_src));
	}

	void JitDspRegPool::movDspReg(const TReg24& _dst, const DspValue& _src) const
	{
		m_block.mem().mov(makeDspPtr(_dst), _src);
	}

	void JitDspRegPool::movDspReg(DspValue& _dst, const TReg24& _src) const
	{
		if (!_dst.isRegValid() || _dst.getBitCount() != 24)
			_dst.temp(DspValue::Temp24);
		m_block.mem().mov(_dst, makeDspPtr(_src));
	}

	void JitDspRegPool::movDspReg(const int8_t& _reg, const JitRegGP& _src) const
	{
		m_block.mem().mov(makeDspPtr(&_reg, sizeof(_reg)), r32(_src));
	}

	void JitDspRegPool::movDspReg(const JitRegGP& _dst, const int8_t& _reg) const
	{
		m_block.mem().mov(r32(_dst), makeDspPtr(&_reg, sizeof(_reg)));
	}

	void JitDspRegPool::parallelOpEpilog(const DspReg _aluReadReg, const DspReg _aluWriteReg)
	{
		if(!isLocked(_aluWriteReg))
			return;

		unlock(_aluWriteReg);

		if(!isInUse(_aluReadReg))
		{
			// because the write reg is already unlocked here, this will automatically copy the write reg to the read reg
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

			auto discardXMM = [&](const bool _writtenReg)
			{
				for (auto it = m_xmList.used().begin(); it != m_xmList.used().end(); ++it)
				{
					const auto dspReg = *it;

					if (dspReg == _wantedReg)
						continue;

					if (isWritten(dspReg) != _writtenReg)
						continue;

					LOGRP("Writing DSP reg " << g_dspRegNames[dspReg] << " back to memory to make space");

					const auto res = release(dspReg);
					assert(res && "unable to release XMM reg");
					return true;
				}
				return false;
			};

			if (!discardXMM(false))
			{
				if(!discardXMM(true))
				{
					LOGRP("Failed to make space, unable to move XMM back to memory");
				}
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

			SpillReg xmReg;
			m_xmList.acquire(xmReg, dspReg, m_repMode);
			m_block.stack().setUsed(xmReg.reg);

			setSpilled(dspReg);

			spillMove(xmReg, hostReg);
			m_moveToXmmInstruction[dspReg] = m_block.asm_().cursor();

			return;
		}
		assert(false && "all GPs are locked, unable to make space");
	}

	void JitDspRegPool::clear()
	{
		m_gpList.clear();
		m_xmList.clear();

		m_lockedGps = DspRegFlags::None;
		m_writtenDspRegs = DspRegFlags::None;
		m_spilledDspRegs = DspRegFlags::None;
		m_loadedDspRegs = DspRegFlags::None;

		for (const auto& g_dspPoolGp : g_dspPoolGps)
			m_gpList.addHostReg(g_dspPoolGp);

		for (const auto& g_dspPoolXmm : g_dspPoolXmms)
			m_xmList.addHostReg({g_dspPoolXmm, 0});

		if(m_extendedSpillSpace)
		{
			for (const auto& g_dspPoolXmm : g_dspPoolXmms)
				m_xmList.addHostReg({g_dspPoolXmm, 1});
		}

		m_availableTemps.clear();

		for(int i=TempA; i<=LastTemp; ++i)
			m_availableTemps.push_back(static_cast<DspReg>(i));
	}

	void JitDspRegPool::load(const JitRegGP& _dst, const DspReg _src)
	{
		setLoaded(_src);

		const auto& r = m_block.dsp().regs();

		auto loadAlu = [this, &_dst](const DspReg _aluRead, const DspReg _aluWrite, const TReg56& _alu)
		{
			if(!isLocked(_aluWrite) && isInUse(_aluWrite))
			{
				move(_dst, _aluWrite);
				if(!m_isParallelOp)
				{
					clearWritten(_aluWrite);
					release(_aluWrite);
					setWritten(_aluRead);
				}
			}
			else
			{
				movDspReg(_dst, _alu);
			}
		};

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
			movDspReg(_dst, r.r[_src - DspR0]);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			movDspReg(_dst, r.n[_src - DspN0]);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			movDspReg(_dst, r.m[_src - DspM0]);
			break;
		case DspM0mod:
		case DspM1mod:
		case DspM2mod:
		case DspM3mod:
		case DspM4mod:
		case DspM5mod:
		case DspM6mod:
		case DspM7mod:
			movDspReg(_dst, r.mModulo[_src - DspM0mod]);
			break;
		case DspM0mask:
		case DspM1mask:
		case DspM2mask:
		case DspM3mask:
		case DspM4mask:
		case DspM5mask:
		case DspM6mask:
		case DspM7mask:
			movDspReg(_dst, r.mMask[_src - DspM0mask]);
			break;
		case DspA:
			loadAlu(DspA, DspAwrite, r.a);
			break;
		case DspAwrite:
			// write only
			break;
		case DspB:
			loadAlu(DspB, DspBwrite, r.b);
			break;
		case DspBwrite:
			// write only
			break;
		case DspX:
			movDspReg(_dst, r.x);
			break;
		case DspY:
			movDspReg(_dst, r.y);
			break;
		case DspPC:
			movDspReg(_dst, r.pc);
			break;
		case DspSR:
			movDspReg(_dst, r.sr);
			break;
		case DspLC: 
			movDspReg(_dst, r.lc);
			break;
		case DspLA: 
			movDspReg(_dst, r.la);
			break;
		}
	}

	void JitDspRegPool::store(const DspReg _dst, const JitRegGP& _src) const
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
			movDspReg(r.r[_dst - DspR0], _src);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			movDspReg(r.n[_dst - DspN0], _src);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			movDspReg(r.m[_dst - DspM0], _src);
			break;
		case DspM0mod:
		case DspM1mod:
		case DspM2mod:
		case DspM3mod:
		case DspM4mod:
		case DspM5mod:
		case DspM6mod:
		case DspM7mod:
			movDspReg(r.mModulo[_dst - DspM0mod], _src);
			break;
		case DspM0mask:
		case DspM1mask:
		case DspM2mask:
		case DspM3mask:
		case DspM4mask:
		case DspM5mask:
		case DspM6mask:
		case DspM7mask:
			movDspReg(r.mMask[_dst - DspM0mask], _src);
			break;
		case DspA:
		case DspAwrite:
			movDspReg(r.a, _src);
			break;
		case DspB:
		case DspBwrite:
			movDspReg(r.b, _src);
			break;
		case DspX:
			movDspReg(r.x, _src);
			break;
		case DspY:
			movDspReg(r.y, _src);
			break;
		case DspPC:
			movDspReg(r.pc, _src);
			break;
		case DspSR:
			movDspReg(r.sr, _src);
			break;
		case DspLC: 
			movDspReg(r.lc, _src);
			break;
		case DspLA: 
			movDspReg(r.la, _src);
			break;
		}
	}

	void JitDspRegPool::store(const DspReg _dst, const SpillReg& _src) const
	{
		auto& r = m_block.dsp().regs();

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
			movDspReg(r.r[_dst - DspR0], _src);
			break;
		case DspN0:
		case DspN1:
		case DspN2:
		case DspN3:
		case DspN4:
		case DspN5:
		case DspN6:
		case DspN7:
			movDspReg(r.n[_dst - DspN0], _src);
			break;
		case DspM0:
		case DspM1:
		case DspM2:
		case DspM3:
		case DspM4:
		case DspM5:
		case DspM6:
		case DspM7:
			movDspReg(r.m[_dst - DspM0], _src);
			break;
		case DspM0mod:
		case DspM1mod:
		case DspM2mod:
		case DspM3mod:
		case DspM4mod:
		case DspM5mod:
		case DspM6mod:
		case DspM7mod:
			movDspReg(r.mModulo[_dst - DspM0mod], _src);
			break;
		case DspM0mask:
		case DspM1mask:
		case DspM2mask:
		case DspM3mask:
		case DspM4mask:
		case DspM5mask:
		case DspM6mask:
		case DspM7mask:
			movDspReg(r.mMask[_dst - DspM0mask], _src);
			break;
		case DspA:
		case DspAwrite:
			movDspReg(r.a, _src);
			break;
		case DspB:
		case DspBwrite:
			movDspReg(r.b, _src);
			break;
		case DspX:
			movDspReg(r.x, _src);
			break;
		case DspY:
			movDspReg(r.y, _src);
			break;
		case DspPC:
			movDspReg(r.pc, _src);
			break;
		case DspSR:
			movDspReg(r.sr, _src);
			break;
		case DspLC: 
			movDspReg(r.lc, _src);
			break;
		case DspLA: 
			movDspReg(r.la, _src);
			break;
		}
	}

	bool JitDspRegPool::release(const DspReg _dst)
	{
		if(isLocked(_dst))
		{
			LOGRP("Unable to release DSP reg " << g_dspRegNames[_dst] << " as its locked");
			return false;
		}

		clearLoaded(_dst);

		JitRegGP gpReg;
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
		
		SpillReg xmReg;
		if(m_xmList.release(xmReg, _dst, m_repMode))
		{
			clearSpilled(_dst);

			if(isWritten(_dst))
			{
				LOGRP("Storing modified DSP reg " << g_dspRegNames[_dst] << " from XMM");
				store(_dst, xmReg);
				clearWritten(_dst);
			}
			else if(m_moveToXmmInstruction[_dst])
			{
				// if a GP was moved to an XMM but the register was never used, we can remove the mov instruction, too
				m_block.asm_().removeNode(m_moveToXmmInstruction[_dst]);
			}

			m_moveToXmmInstruction[_dst] = nullptr;
		}

		return true;
	}
	
	bool JitDspRegPool::flagSet(DspRegFlags& _flags, const DspReg _reg)
	{
		const auto last = _flags;
		_flags = static_cast<DspRegFlags>(static_cast<uint64_t>(_flags) | (1ull << static_cast<uint64_t>(_reg)));
		return _flags != last;
	}

	bool JitDspRegPool::flagClear(DspRegFlags& _flags, const DspReg _reg)
	{
		const auto last = _flags;
		_flags = static_cast<DspRegFlags>(static_cast<uint64_t>(_flags) & ~(1ull << static_cast<uint64_t>(_reg)));
		return _flags != last;
	}

	bool JitDspRegPool::flagTest(const DspRegFlags& _flags, const DspReg _reg)
	{
		return static_cast<uint64_t>(_flags) & (1ull<<static_cast<uint64_t>(_reg));
	}

	JitMemPtr JitDspRegPool::makeDspPtr(const void* _ptr, const size_t _size) const
	{
		const void* base = &m_block.dsp().regs();

		const auto p = Jitmem::makeRelativePtr(_ptr, base, regDspPtr, _size);

		if(!p.hasSize())
		{
			m_dspPtr.setSize(0);
			m_dspPtr.setOffset(0);
			return m_dspPtr;
		}

		if(!m_dspPtr.hasBase() || !m_dspPtr.hasSize())
		{
			m_block.stack().setUsed(regDspPtr);
			m_block.asm_().mov(regDspPtr, asmjit::Imm(reinterpret_cast<uint64_t>(base)));
			m_dspPtr = Jitmem::makePtr(regDspPtr, static_cast<uint32_t>(_size));
		}

		m_dspPtr.setSize(p.size());
		m_dspPtr.setOffset(p.offset());

		return m_dspPtr;
	}

	void JitDspRegPool::reset()
	{
		clear();
		m_moveToXmmInstruction.fill(nullptr);
		m_isParallelOp = false;
		m_repMode = false;
		m_dspPtr.reset();
		m_dirty = false;
	}

	void JitDspRegPool::mov(const JitMemPtr& _dst, const JitRegGP& _src) const
	{
		m_block.mem().mov(_dst, _src);
	}

	void JitDspRegPool::movd(const JitMemPtr& _dst, const SpillReg& _src) const
	{
//		if(_src.offset == 0)
			m_block.asm_().movd(_dst, _src.reg);
//		else
//			m_block.asm_().pextrd(_dst, _src.reg, asmjit::Imm(_src.offset<<1));
	}

	void JitDspRegPool::movq(const JitMemPtr& _dst, const SpillReg& _src) const
	{
//		if(_src.offset == 0)
			m_block.asm_().movq(_dst, _src.reg);
//		else
//			m_block.asm_().pextrq(_dst, _src.reg, asmjit::Imm(_src.offset));
	}

	void JitDspRegPool::mov(const JitRegGP& _dst, const JitMemPtr& _src) const
	{
		m_block.mem().mov(_dst, _src);
	}

	void JitDspRegPool::movd(const JitReg128& _dst, const JitMemPtr& _src) const
	{
		m_block.asm_().movd(_dst, _src);
	}

	void JitDspRegPool::movq(const JitReg128& _dst, const JitMemPtr& _src) const
	{
		m_block.asm_().movq(_dst, _src);
	}

	void JitDspRegPool::spillMove(const JitRegGP& _dst, const SpillReg& _src) const
	{
//		if(_src.offset == 0)
			m_block.asm_().movq(r64(_dst), _src.reg);
//		else
//			m_block.asm_().pextrq(r64(_dst), _src.reg, asmjit::Imm(_src.offset));
	}

	void JitDspRegPool::spillMove(const SpillReg& _dst, const JitRegGP& _src) const
	{
		// if(m_extendedSpillSpace)
		// 	m_block.asm_().pinsrq(_dst.reg, r64(_src), asmjit::Imm(_dst.offset));
		// else
			m_block.asm_().movq(_dst.reg, r64(_src));
	}

	void JitDspRegPool::spillMove(const SpillReg& _dst, const SpillReg& _src) const
	{
		if(m_extendedSpillSpace)
		{
			if(_dst.offset == 0 && _src.offset == 0)
			{
				m_block.asm_().movq(_dst.reg, _src.reg);
			}
			else
			{
				const RegScratch s(m_block);
				spillMove(s, _src);
				spillMove(_dst, s);
			}
		}
		else
		{
			m_block.asm_().movdqa(_dst.reg, _src.reg);
		}
	}
}
