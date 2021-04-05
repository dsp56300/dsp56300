#pragma once

#include <list>
#include <string>

#include "types.h"

namespace dsp56k
{
	class Memory;

	class OMFLoader
	{
		// _____________________________________________________________________________
		// members
		//
	private:
		EMemArea	m_currentArea = MemArea_COUNT;
		TWord		m_currentTargetAddress = 0;
		size_t		m_currentBitSize = 0;
		size_t		m_currentByteSize = 0;

		char		m_currentSymbolArea = 0;

		// _____________________________________________________________________________
		// implementation
		//
	public:

		bool load( const char* _filename				, Memory& _dst );
		bool load( const std::string& _istr				, Memory& _dst );
		bool load( const std::list<std::string>& _lines	, Memory& _dst );

		// _____________________________________________________________________________
		// helpers
		//
	private:
		void				parseLine	( const std::string& _line, Memory& _dst );
		static unsigned int	parse24Bit	( const char* _src );
	};
};
