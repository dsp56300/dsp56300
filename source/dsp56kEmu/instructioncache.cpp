#include "pch.h"

#include "instructioncache.h"
#include "memory.h"

namespace dsp56k
{
	const TWord g_initPattern = 0xffabcabc;

	// _____________________________________________________________________________
	// reset
	//
	void InstructionCache::reset()
	{
		for( TSectorIdx s=0; s<eNumSectors; ++s )
		{
			m_lruStack[s] = eNumSectors-s-1;

			flushSector( s );
		}
	}

	// _____________________________________________________________________________
	// flushSector
	//
	void InstructionCache::flushSector( TSectorIdx s )
	{
		m_sectorLocked	[s] = false;
		m_tagRegister	[s] = eMaskTAG;

		for( TWord i=0; i<eSectorSize; ++i )
		{
			m_validBits	[s][i] = false;
			m_memory	[s][i] = g_initPattern;
		}
	}

	// _____________________________________________________________________________
	// fetch
	//
	dsp56k::TWord InstructionCache::fetch( Memory& _mem, TWord _address, bool _burst )
	{
		return _mem.get( MemArea_P, _address );

		TSectorIdx s = createSectorForAddress( _address );

		if( s == eSectorInvalid )
		{
			// all sectors locked and the specified address is not part of cache
			return _mem.get( MemArea_P, _address );
		}

		TWord word = _address & eMaskVBIT;

		if( m_validBits[s][word] )
		{
			// if cache entry is valid, return cached value. 
			return m_memory[s][word];
		}
		else
		{
			// ... otherwise, return value of memory and store it in cache

			// if burst mode is enabled, fetch more than only one word from memory
			TWord fetchCount = 1;

			if( _burst )
			{
				// no breaks in this switch by design
				switch( _address & 0x000003 )
				{
				case 0:
					m_memory   [s][word+3] = _mem.get( MemArea_P, _address+3 );
					m_validBits[s][word+3] = true;
				case 1:
					m_memory   [s][word+2] = _mem.get( MemArea_P, _address+2 );
					m_validBits[s][word+2] = true;
				case 2:
					m_memory   [s][word+1] = _mem.get( MemArea_P, _address+1 );
					m_validBits[s][word+1] = true;
				case 3:
					m_memory   [s][word  ] = _mem.get( MemArea_P, _address );
					m_validBits[s][word  ] = true;
				}
			}
			else
			{
					m_memory   [s][word  ] = _mem.get( MemArea_P, _address );
					m_validBits[s][word  ] = true;
			}

			return m_memory[s][word];
		}
	}

	// _____________________________________________________________________________
	// findLRUSector
	//
	InstructionCache::TSectorIdx InstructionCache::findLRUSector()
	{
		for( int i=eNumSectors-1; i>=0; --i )
		{
			TSectorIdx s = m_lruStack[i];
			if( !m_sectorLocked[s] )
				return s;
		}
		return eSectorInvalid;
	}

	// _____________________________________________________________________________
	// setSectorMRU
	//
	void InstructionCache::setSectorMRU( TSectorIdx _s )
	{
		if( m_lruStack[0] == _s )
			return;	// no change required

		TSectorIdx temp = m_lruStack[0];

		m_lruStack[0] = _s;

		// shift sectors upward until our own sector is found, that is placed into 0
		for( unsigned int i=1; i<m_lruStack.size(); ++i )
		{
			if( m_lruStack[i] == _s )
			{
				m_lruStack[i] = temp;
				return;
			}
			std::swap( temp, m_lruStack[i] );
		}

	}
	// _____________________________________________________________________________
	// lock
	//
	bool InstructionCache::plock( TWord _address )
	{
		TSectorIdx s = createSectorForAddress(_address);

		if( s == eSectorInvalid )
		{
			return false;
		}

		m_sectorLocked[s] = true;

		return true;
	}

	// _____________________________________________________________________________
	// initializeSector
	//
	bool InstructionCache::initializeSector( TSectorIdx s, TWord _address )
	{
		const TWord tag = _address & eMaskTAG;

		m_tagRegister[s] = tag;

		for( int i=0; i<m_memory[s].eSize; ++i )
		{
			m_validBits[s][i] = false;
			m_memory[s][i] = g_initPattern;
		}

		return true;
	}

	// _____________________________________________________________________________
	// createSectorForAddress
	//
	InstructionCache::TSectorIdx InstructionCache::createSectorForAddress( TWord _address )
	{
		TSectorIdx s = findSectorByAddress( _address );

		if( s != eSectorInvalid )
		{
			// sector already cached
			setSectorMRU(s);
			return s;
		}

		s = findLRUSector();

		if( s == eSectorInvalid )
		{
			// all sectors already locked and in use by other memory areas
			return eSectorInvalid;
		}

		setSectorMRU(s);
		initializeSector( s, _address );
		return s;	
	}
	// _____________________________________________________________________________
	// pfree
	//
	void InstructionCache::pfree()
	{
		for( int i=0; i<eNumSectors; ++i )
			m_sectorLocked[i] = false;
	}
	// _____________________________________________________________________________
	// pflushun
	//
	void InstructionCache::pflushun()
	{
		// TODO
	}
}
