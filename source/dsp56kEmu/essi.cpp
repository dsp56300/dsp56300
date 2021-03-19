#include "pch.h"

#include "essi.h"

#include "memory.h"
#include "utils.h"

namespace dsp56k
{
	// _____________________________________________________________________________
	// exec
	//
	void Essi::exec()
	{
		return;

		// i don't know what it does but my code is waiting for a change of this bit...

		TWord val = m_memory.get( MemArea_X, 0xffffb3 );

		if( m_toggleState )
		{
			bittestandset( val, 0xd );
		}
		else
		{
			bittestandclear( val, 0xd );
		}
		m_toggleState = !m_toggleState;

		m_memory.set( MemArea_X, 0xffffb3, val );
	}
};
