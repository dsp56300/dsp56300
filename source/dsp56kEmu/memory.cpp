#include "pch.h"

#include "memory.h"
#include "error.h"
#include "omfloader.h"

namespace dsp56k
{
	static const TWord g_initPattern = 0xabcabcab;

	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory()
#ifdef _DEBUG
		: x(m_mem[MemArea_X])
		, y(m_mem[MemArea_Y])
		, p(m_mem[MemArea_P])
#endif
	{
		for( size_t i=0; i<MemArea_COUNT; ++i )
			m_mem[i].resize( 0xC0000, 0 );
	}

	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory( const Memory& _src )
#ifdef _DEBUG
		: x(m_mem[MemArea_X])
		, y(m_mem[MemArea_Y])
		, p(m_mem[MemArea_P])
#endif
	{
		m_mem = _src.m_mem;

		m_perif[MemArea_X] = _src.m_perif[MemArea_X];
		m_perif[MemArea_Y] = _src.m_perif[MemArea_Y];
	}

	// _____________________________________________________________________________
	// ~Memory
	//
	Memory::~Memory()
	{
	}

	// _____________________________________________________________________________
	// set
	//
	bool Memory::set( EMemArea _area, TWord _offset, TWord _value )
	{
		translateAddress( _area, _offset );

		if( _offset == 0x04d1e6 )
		{
			if( _area == MemArea_X )
			{
				int d=0;
			}
		}

		if( (_area <= MemArea_Y) && m_perif[_area].isValidAddress(_offset) )
		{
			m_perif[_area].write( _offset, _value );
			return true;
		}

		if( _offset >= m_mem[_area].size() )
		{
			LOG_ERR_MEM_WRITE( _offset );
			return false;
		}

		m_mem[_area][_offset] = (_value & 0x00ffffff);

		return true;
	}

	// _____________________________________________________________________________
	// get
	//
	TWord Memory::get( EMemArea _area, TWord _offset ) const
	{
		translateAddress( _area, _offset );

		if( _offset == 0x000201 )
		{
			if( _area == MemArea_X )
			{
				int d=0;
			}
		}

		if( (_area <= MemArea_Y) && m_perif[_area].isValidAddress(_offset) )
		{
			return m_perif[_area].read(_offset);
		}

		if( _offset >= m_mem[_area].size() )
		{
			LOG_ERR_MEM_READ( _offset );
			assert( 0 && "invalid memory address" );
			return 0x00badbad;
		}

		const TWord res = m_mem[_area][_offset];

		if( res == g_initPattern && _offset > 9 )
			LOG_ERR_MEM_READ_UNINITIALIZED(_area,_offset);

		return res;
	}
	// _____________________________________________________________________________
	// loadOMF
	//
	bool Memory::loadOMF( const std::string& _filename )
	{
		OMFLoader loader;
		return loader.load( _filename, *this );
	}
	// _____________________________________________________________________________
	// fillWithInitPattern
	//
	void Memory::fillWithInitPattern()
	{
		for( size_t i=0; i<MemArea_COUNT; ++i )
		{
			for( size_t a=0; a<m_mem[i].size(); ++a )
			{
				m_mem[i][a] = g_initPattern;
			}
		}
	}
	// _____________________________________________________________________________
	// translateAddress
	//
	bool Memory::translateAddress( EMemArea& _area, TWord& _offset ) const
	{
		// XXX maps external memory to XYP areas at once
		if( _offset >= 0x040000 && _offset < 0x0c0000 && (_area == MemArea_Y || _area == MemArea_P) )
		{
			_area = MemArea_X;
			return true;
		}

		// address not modified
		return false;
	}
}
