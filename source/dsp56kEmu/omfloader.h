#pragma once

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
		EMemArea	m_currentArea;
		TWord		m_currentTargetAddress;
		size_t		m_currentBitSize;
		size_t		m_currentByteSize;

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
		void			parseLine	( const std::string& _line, Memory& _dst );
		unsigned int	parse24Bit	( const char* _src );
		void			parse24Bit	( const char* _src, unsigned char* _dst );
	};
};
