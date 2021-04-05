#pragma once

#include "staticArray.h"
#include "types.h"

namespace dsp56k
{
	class Memory;

	class InstructionCache
	{
		// _____________________________________________________________________________
		// types
		//
		enum { eTotalSize	= 1024		};
		enum { eNumSectors	= 8			};
		enum { eSectorSize	= 128		};
		enum { eSectorInvalid = eNumSectors };

		enum { eMaskVBIT	= 0x00007f	};
		enum { eMaskTAG		= 0xffff80	};


		typedef unsigned int TSectorIdx;

		// _____________________________________________________________________________
		// members
		//

		// TAG fields for every sector
		StaticArray< StaticArray< TWord, eSectorSize >, eNumSectors >	m_memory;
		StaticArray< StaticArray< bool , eSectorSize >, eNumSectors >	m_validBits;

		StaticArray< TWord     , eNumSectors >							m_tagRegister;
		StaticArray< bool      , eNumSectors >							m_sectorLocked;

		StaticArray< TSectorIdx, eNumSectors >							m_lruStack;	// 0 = MRU / 7 = LRU

		// _____________________________________________________________________________
		// implementation
		//
	public:
		InstructionCache	()															{ reset(); }

		void	reset		();

		bool	plock		( TWord _address );

		void	pfree		();

		void	pflushun	();

		TWord	fetch		( Memory& _mem, TWord _address, bool burstEnabled );

		TWord	readMemory	( TSectorIdx _sector, TWord _wordIdx ) const
		{
			if( _sector >= eNumSectors || _wordIdx >= eSectorSize )
			{
				assert( 0 && "invalid address" );
				return 0;
			}
			return m_memory[_sector][_wordIdx];
		}

		TWord	readMemory	( TWord _address ) const
		{
			TSectorIdx	s		= _address >> 7;
			TWord		wordIdx	= _address - (s<<7);

			return readMemory( s, wordIdx );
		}

		// _____________________________________________________________________________
		// 
		//
	private:

		void flushSector( TSectorIdx s );

		TSectorIdx	findSectorByAddress( TWord _addr ) const
		{
			const TWord check = _addr & eMaskTAG;

			for( int i=0; i<m_tagRegister.eSize; ++i )
			{
				if( m_tagRegister[i] == check )
					return i;
			}
			return eSectorInvalid;
		}

		// find least recently used sector, returns eSectorInvalid if none was found (i.e. all sectors are locked)
		TSectorIdx	findLRUSector();

		// marks a sector as most recently used
		void		setSectorMRU( TSectorIdx _s );

		bool		initializeSector( TSectorIdx s, TWord _address );

		TSectorIdx	createSectorForAddress( TWord _address );
	};
}
