#include "memory.h"

#include <iomanip>

#include "error.h"
#include "omfloader.h"

namespace dsp56k
{
	static const TWord g_initPattern = 0xabcabcab;

	static DefaultMemoryMap g_defaultMemoryMap;
	
	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory(IPeripherals* _peripheralsX, IPeripherals* _peripheralsY, const IMemoryMap* _memoryMap, size_t _memSize/* = 0xc00000*/)
		: m_memoryMap(_memoryMap ? _memoryMap : &g_defaultMemoryMap)
		, x(m_mem[MemArea_X])
		, y(m_mem[MemArea_Y])
		, p(m_mem[MemArea_P])
		, m_perif({_peripheralsX, _peripheralsY})
		, m_dsp(nullptr)
	{
		for( size_t i=0; i<MemArea_COUNT; ++i )
			m_mem[i].resize( _memSize, 0 );
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
		m_memoryMap->memTranslateAddress(_area, _offset);

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap->memValidateAccess(_area, _offset, true))
			return false;

		if( _offset >= m_mem[_area].size() )
		{
			LOG_ERR_MEM_WRITE( _offset );
			return false;
		}
#endif
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
		m_mem[_area][_offset] = _value & 0x00ffffff;

		return true;
	}

	// _____________________________________________________________________________
	// get
	//
	TWord Memory::get( EMemArea _area, TWord _offset ) const
	{
		m_memoryMap->memTranslateAddress(_area, _offset);

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap->memValidateAccess(_area, _offset, true))
			return false;

		if( _offset >= m_mem[_area].size() )
		{
			LOG_ERR_MEM_READ( _offset );
			assert( 0 && "invalid memory address" );
			return 0x00badbad;
		}
#endif

		const auto res = m_mem[_area][_offset];

#ifdef _DEBUG
		if( res == g_initPattern)
			LOG_ERR_MEM_READ_UNINITIALIZED(_area,_offset);
#endif

		return res;
	}

	bool Memory::setPeriph(EMemArea _area, TWord _offset, TWord _value)
	{
		m_perif[_area]->write(_offset, _value);
		return true;
	}

	bool Memory::setPeriphFFFF80(const EMemArea _area, const TWord _offset, const TWord _value)
	{
		return setPeriph(_area, _offset + 0xffff80, _value);
	}

	bool Memory::setPeriphFFFFC0(const EMemArea _area, const TWord _offset, const TWord _value)
	{
		return setPeriph(_area, _offset + 0xffffc0, _value);
	}

	TWord Memory::getPeriph(const EMemArea _area, const TWord _offset) const
	{
		return m_perif[_area]->read(_offset);
	}

	TWord Memory::getPeriphFFFF80(const EMemArea _area, const TWord _offset) const
	{
		return getPeriph(_area, _offset + 0xffff80);
	}

	TWord Memory::getPeriphFFFFC0(const EMemArea _area, const TWord _offset) const
	{
		return getPeriph(_area, _offset + 0xffffc0);
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
	// save
	//
	bool Memory::save( FILE* _file ) const
	{
		for( size_t i=0; i<m_mem.size(); ++i )
		{
			const std::vector<TWord>& data = m_mem[i];
			fwrite( &data[0], sizeof( data[0] ), data.size(), _file );
		}

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

	const std::string& Memory::getSymbol(EMemArea _memArea, const TWord addr)
	{
		static std::string empty;

		auto c = g_memAreaNames[_memArea];

		const auto it = m_symbols.find(c);
		if(it == m_symbols.end())
			return empty;
		const auto it2 = it->second.find(addr);
		if(it2 == it->second.end())
			return empty;
		return *it2->second.names.begin();
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
