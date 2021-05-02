#include "memory.h"


#include <fstream>
#include <iomanip>


#include "disasm.h"
#include "error.h"
#include "omfloader.h"

namespace dsp56k
{
	constexpr bool g_useInitPattern	= false;
	constexpr TWord g_initPattern	= 0xabcabcab;

	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory(const IMemoryValidator& _memoryMap, size_t _memSize/* = 0xc00000*/, TWord* _externalBuffer/* = nullptr*/)
		: m_memoryMap(_memoryMap)
		, m_size(_memSize)
		, m_dsp(nullptr)
	{
		auto* address = _externalBuffer;

		if(!address)
		{
			m_buffer.resize(_memSize * MemArea_COUNT, 0);
			address = &m_buffer[0];
		}

		p = address;	address += size();
		x = address;	address += size();
		y = address;

		m_mem[MemArea_X] = x;
		m_mem[MemArea_Y] = y;
		m_mem[MemArea_P] = p;

#if MEMORY_HEAT_MAP
		for(size_t i=0; i<MemArea_COUNT; ++i)
			m_heatMap[i].resize(size());
#endif

		if(g_useInitPattern)
			fillWithInitPattern();
	}

	// _____________________________________________________________________________
	// set
	//
	bool Memory::dspWrite( EMemArea& _area, TWord& _offset, TWord _value )
	{
#if MEMORY_HEAT_MAP
		++m_heatMap[_area][_offset];
#endif

		memTranslateAddress(_area, _offset);

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap.memValidateAccess(_area, _offset, true))
			return false;

		if( _offset >= size() )
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
		if (_offset<0xff0000)	// Fix the amazing "write to wrong address" bug.
		m_mem[_area][_offset] = _value & 0x00ffffff;

		return true;
	}

	// _____________________________________________________________________________
	// get
	//
	TWord Memory::get( EMemArea _area, TWord _offset ) const
	{
#if MEMORY_HEAT_MAP
		++m_heatMap[_area][_offset];
#endif

		memTranslateAddress(_area, _offset);

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap.memValidateAccess(_area, _offset, true))
			return false;

		if( _offset >= size() )
		{
			LOG_ERR_MEM_READ( _offset );
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

	void Memory::get2(EMemArea _area, TWord _offset, TWord& _wordA, TWord& _wordB)
	{
#if MEMORY_HEAT_MAP
		++m_heatMap[_area][_offset];
		++m_heatMap[_area][_offset+1];
#endif

		memTranslateAddress(_area, _offset);
		
#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap.memValidateAccess(_area, _offset, true))
		{
			_wordA = _wordB =  0x00badbad;
			return;			
		}

		if( _offset >= size() )
		{
			LOG_ERR_MEM_READ( _offset );
			assert( 0 && "invalid memory address" );
			_wordA = _wordB =  0x00badbad;
			return;
		}
#endif

		_wordA = m_mem[_area][_offset];
		_wordB = m_mem[_area][_offset+1];

#ifdef _DEBUG
		if( _wordA == g_initPattern || _wordB == g_initPattern)
			LOG_ERR_MEM_READ_UNINITIALIZED(_area,_offset);
#endif
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
		for(size_t a=0; a<m_mem.size(); ++a)
		{
			const auto& data = m_mem[a];
			fwrite( &data[0], sizeof( data[0] ), size(), _file );
		}
		return true;
	}

	// _____________________________________________________________________________
	// load
	//
	bool Memory::load( FILE* _file )
	{
		for(size_t a=0; a<m_mem.size(); ++a)
		{
			const auto& data = m_mem[a];
			fread( &data[0], sizeof( data[0] ), size(), _file );
		}
		return true;
	}

	bool Memory::save(const char* _file, EMemArea _area) const
	{
		FILE* hFile = fopen(_file, "wb");
		if(!hFile)
			return false;

		std::vector<uint8_t> buf;
		buf.resize(size() * 3);

		std::ofstream out(_file, std::ios::binary | std::ios::trunc);

		if(!out.is_open())
			return false;

		size_t index = 0;

		for(uint32_t i=0; i<size(); ++i)
		{
			const auto w = get(_area, i);

			buf[index++] =(w>>16) & 0xff;;
			buf[index++] =(w>>8) & 0xff;;
			buf[index++] =(w) & 0xff;;
		}

		out.write(reinterpret_cast<const char*>(&buf.front()), size() * 3);

		out.close();

		return true;
	}

	bool Memory::saveAssembly(const char* _file, TWord _offset, const TWord _count, bool _skipNops, bool _skipDC, IPeripherals* _peripherals)
	{
		std::ofstream out(_file, std::ios::trunc);

		if(!out.is_open())
			return false;

		Opcodes opcodes;
		Disassembler disasm(opcodes);

		disasm.addSymbols(*this);
		
		if(_peripherals)
			_peripherals->setSymbols(disasm);

		const auto& pSymbols = disasm.getSymbols(Disassembler::MemP);

		for(uint32_t i=_offset; i<_offset+_count;)
		{
			TWord opA, opB;
			get2(MemArea_P, i, opA, opB);

			if(!opA && _skipNops)
			{
				++i;
				continue;
			}

			const auto it = pSymbols.find(i);
			if(it != pSymbols.end())
				out << it->second << ':' << std::endl;

			std::string assembly;
			auto usedOps = disasm.disassemble(assembly, opA, opB, 0, 0, i);

			if(usedOps == 0)
			{
				++usedOps;

				if(_skipDC)
				{
					++i;
					continue;
				}
			}

			std::stringstream o;
			o << HEX(i) << ": " << assembly;

			std::string line(o.str());

			while(line.size() < 60)
				line += ' ';

			out << line << "; ";
			out << HEX(opA);

			if(usedOps > 1)
				out << ' ' << HEX(opB);

			out << std::endl;

			i += usedOps;
		}
		return true;
	}

	bool Memory::saveAsText(const char* _file, EMemArea _area, const TWord _offset, const TWord _count)
	{
		std::ofstream out(_file, std::ios::trunc);
		if(!out.is_open())
			return false;

		auto putVal = [&](TWord v)
		{
			std::stringstream ss;
			ss << std::setfill('0') << std::hex << std::setw(8) << v;
			const std::string str(ss.str());
			return str;
		};

		const auto last = _offset+_count;

		for(auto i=_offset; i<last; i += 8)
		{
			out << "0x" << putVal(i) << ':';

			for(uint32_t j=i; j<i+8 && j < last; ++j)
			{
				out << ' ' << putVal(get(_area, j));
			}

			out << std::endl;
		}
		return true;
	}

	bool Memory::saveHeatmap(const char* _file, bool _writeZeroes)
	{
#if MEMORY_HEAT_MAP
		std::ofstream out(_file, std::ios::trunc);
		if(!out.is_open())
			return false;

//		for(size_t a=0; a<m_heatMap.size(); ++a)
		const size_t a = MemArea_P;
		{
			const auto& hm = m_heatMap[a];
			for(size_t i=0; i<hm.size(); ++i)
			{
				const auto count = hm[i];
				if(!count && !_writeZeroes)
					continue;
				out << g_memAreaNames[a] << ':' << HEX(i) << ": " << std::setfill('0') << std::setw(8) << std::hex << count << std::endl;
			}
		}

		return true;
#else
		return false;
#endif
	}
	bool Memory::saveHeatmapImage(const char* _file)
	{
#if MEMORY_HEAT_MAP
		std::ofstream out(_file, std::ios::trunc);
		if(!out.is_open())
			return false;

		const uint32_t width = 512;
		const uint32_t height = size() / 512;

		out << "P3" << std::endl;
		out << width << ' ' << height << std::endl;
		out << "255" << std::endl;

		const size_t a = MemArea_P;

		const auto& hm = m_heatMap[a];
		size_t maxVal = 0;
		for(size_t i=0; i<hm.size(); ++i)
			maxVal = std::max(maxVal, (size_t)hm[i]);

		for(size_t i=0; i<hm.size(); ++i)
		{
			if(hm[i] == 0)
				out << "255 0 0";
			else
			{
				const int brightness = hm[i] * 255 / maxVal;
				out << "0 " << brightness << " 0";				
			}

			if(i % width == (width-1))
				out << std::endl;
			else
				out << ' ';
		}

		return true;
#else
		return false;
#endif
	}
	void Memory::clearHeatmap()
	{
#if MEMORY_HEAT_MAP
		for (size_t a=0; a<MemArea_COUNT; ++a)
		{
			m_heatMap[a].clear();
			m_heatMap[a].resize(size());
		}
#endif
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
		for(size_t a=0; a<m_mem.size(); ++a)
		{
			for(size_t i=0; i<size(); ++i)
				m_mem[a][i] = g_initPattern;
		}
	}

	void Memory::memTranslateAddress(EMemArea& _area, TWord& _addr) const
	{
//		if(_addr >= m_bridgedMemoryAddress)
//			_area = MemArea_P;

		// It's magic...
		auto o = static_cast<int32_t>(_addr - m_bridgedMemoryAddress);
		o >>= 24;
		_area = static_cast<EMemArea>(_area & o);
//		assert(_area >= 0 && _area < MemArea_COUNT);
	}
}
