#include "pch.h"

#include "memory.h"

#include <iomanip>

#include "error.h"
#include "omfloader.h"
#include "dsp.h"

namespace dsp56k
{
	static const TWord g_initPattern = 0xabcabcab;

	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory()
		: x(m_mem[MemArea_X])
		, y(m_mem[MemArea_Y])
		, p(m_mem[MemArea_P])
		, m_dsp(0)
	{
		for( size_t i=0; i<MemArea_COUNT; ++i )
			m_mem[i].resize( 0xC0000, 0 );
	}

	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory( const Memory& _src )
		: x(m_mem[MemArea_X])
		, y(m_mem[MemArea_Y])
		, p(m_mem[MemArea_P])
		, m_dsp(0)
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
/*
		if( m_dsp && m_dsp->getICTR() )
		{
			const auto oldValue = m_mem[_area][_offset];

			if(oldValue != _value)
			{
				STransaction trans;
				trans.area = _area;
				trans.ictr = m_dsp->getICTR();
				trans.newVal = _value;
				trans.offset = _offset;
				trans.oldVal = m_mem[_area][_offset];
				m_transactionHistory.push_back( trans );

				LOGF( "MEMCHANGE " << g_memAreaNames[_area] << ":" << std::hex << _offset << ", " << std::hex << trans.oldVal << " => " << std::hex << trans.newVal << ", ictr " << trans.ictr );
			}
		}
*/
		m_mem[_area][_offset] = (_value & 0x00ffffff);

		return true;
	}

	// _____________________________________________________________________________
	// get
	//
	TWord Memory::get( EMemArea _area, TWord _offset ) const
	{
		translateAddress( _area, _offset );

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

		if( res == g_initPattern)
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

	// _____________________________________________________________________________
	// save
	//
	bool Memory::save( FILE* _file ) const
	{
		for( size_t i=0; i<m_mem.size(); ++i )
		{
			const std::vector<TWord>& data = m_mem[i];
			fwrite( &data[0], sizeof( data[0] ), data.size(), _file );
		}

		// some kind of dirty for sure...
		fwrite( &m_perif[0], sizeof(m_perif[0]), m_perif.size(), _file );

		return true;
	}

	// _____________________________________________________________________________
	// load
	//
	bool Memory::load( FILE* _file )
	{
		for( size_t i=0; i<m_mem.size(); ++i )
		{
			std::vector<TWord>& data = m_mem[i];
			fread( &data[0], sizeof( data[0] ), data.size(), _file );
		}

		// some kind of dirty for sure...
		fread( &m_perif[0], sizeof(m_perif[0]), m_perif.size(), _file );

		return true;
	}

	void Memory::setSymbol(char _area, TWord _address, const std::string& _name)
	{
		SSymbol s;
		s.address = _address;
		s.area = _area;
		s.names.insert(_name);

		const auto itArea = m_symbols.find(_area);

		if(itArea == m_symbols.end())
		{
			std::map<TWord, SSymbol> symbols;
			symbols.insert(std::make_pair(_address, s));
			m_symbols.insert(std::make_pair(_area, symbols));
		}
		else
		{
			auto& symbols = itArea->second;

			auto itAddress = symbols.find(_address);
			if(itAddress == symbols.end())
			{
				symbols.insert(std::make_pair(_address, s));				
			}
			else
			{
				auto& symbol = itAddress->second;

				if(symbol.names.find(_name) != symbol.names.end())
				{
					symbol.names.insert(_name);
				}
			}
		}
	}

	// _____________________________________________________________________________
	// fillWithInitPattern
	//
	void Memory::fillWithInitPattern()
	{
		for( size_t i=0; i<MemArea_COUNT; ++i )
		{
			for (TWord& a : m_mem[i])
			{
				a = g_initPattern;
			}
		}
	}

}
