#include "memory.h"


#include <fstream>
#include <iomanip>

#include "disasm.h"
#include "dsp.h"
#include "error.h"
#include "omfloader.h"

namespace dsp56k
{
	constexpr bool g_useInitPattern	= false;
	constexpr TWord g_initPattern	= 0xabcabcab;

	// _____________________________________________________________________________
	// Memory
	//
	Memory::Memory(const IMemoryValidator& _memoryMap, TWord _memSize/* = 0xc00000*/, TWord* _externalBuffer/* = nullptr*/)
		: m_memoryMap(_memoryMap)
		, m_size({_memSize, _memSize, _memSize})
		, m_mem({nullptr})
		, m_bridgedMemoryAddress(_memSize)
		, m_dsp(nullptr)
	{
		auto* address = _externalBuffer;

		if(!address)
		{
			m_buffer.resize(static_cast<size_t>(_memSize) * MemArea_COUNT, 0);
			address = &m_buffer[0];
		}

		p = address;	address += sizeP();
		x = address;	address += sizeXY();
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

	Memory::Memory(const IMemoryValidator& _memoryMap, TWord _memSizeP, TWord _memSizeXY, TWord _brigedMemoryAddress/* = 0*/, TWord* _externalBuffer/* = nullptr*/)
		: m_memoryMap(_memoryMap)
		, m_size({_memSizeP, _memSizeXY, _memSizeXY})
		, m_mem({nullptr})
		, m_bridgedMemoryAddress(_brigedMemoryAddress)
		, m_dsp(nullptr)
	{
		// As XY is bridged to P for all addresses >= _brigedMemoryAddress, we need to allocate more for P but less for XY if a bridged address is specified
		const auto pSize = calcPMemSize(_memSizeP, _memSizeXY, _brigedMemoryAddress);
		const auto xySize = calcXYMemSize(_memSizeXY, _brigedMemoryAddress);

		m_mmuBuffer.reset(new MemoryBuffer(pSize, xySize, _brigedMemoryAddress));

		if(m_mmuBuffer->isValid())
		{
			x = m_mmuBuffer->ptrX();
			y = m_mmuBuffer->ptrY();
			p = m_mmuBuffer->ptrP();
		}
		else
		{
			m_mmuBuffer.reset();
			auto* address = _externalBuffer;

			if(!address)
			{
				m_buffer.resize(calcMemSize(_memSizeP, _memSizeXY, _brigedMemoryAddress), 0);
				address = &m_buffer[0];
			}

			// try to keep internal XY and P addresses as close together as possible
			if(xySize < pSize)
			{
				x = address;	address += xySize;
				y = address;	address += xySize;
				p = address;
			}
			else
			{
				p = address;	address += pSize;
				x = address;	address += xySize;
				y = address;
			}
		}

		m_mem[MemArea_X] = x;
		m_mem[MemArea_Y] = y;
		m_mem[MemArea_P] = p;
	}

	// _____________________________________________________________________________
	// set
	//
	bool Memory::dspWrite( EMemArea& _area, TWord& _offset, TWord _value )
	{
#if MEMORY_HEAT_MAP
		++m_heatMap[_area][_offset];
#endif

#if DSP56300_DEBUGGER
		if(m_dsp->getDebugger())
			m_dsp->getDebugger()->onMemoryWrite(_area, _offset, _value);
#endif

		memTranslateAddress(_area, _offset);

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap.memValidateAccess(_area, _offset, true))
			return false;

		if( _offset >= size(_area) )
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
		if (_offset < size(_area))		// Fix the amazing "write to wrong address" bug
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

#if DSP56300_DEBUGGER
		if(m_dsp->getDebugger())
			m_dsp->getDebugger()->onMemoryRead(_area, _offset);
#endif

		memTranslateAddress(_area, _offset);

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap.memValidateAccess(_area, _offset, true))
			return 0;
#endif
		if( _offset >= size(_area) )
		{
			LOG_ERR_MEM_READ( _offset );
			return 0;
		}

		const auto res = m_mem[_area][_offset];

#ifdef _DEBUG
		if( res == g_initPattern)
			LOG_ERR_MEM_READ_UNINITIALIZED(_area,_offset);
#endif

		return res;
	}

	void Memory::getOpcode(TWord _offset, TWord& _wordA, TWord& _wordB) const
	{
#if MEMORY_HEAT_MAP
		++m_heatMap[MemArea_P][_offset];
#endif

#ifdef _DEBUG
		assert(_offset < XIO_Reserved_High_First);
		if(!m_memoryMap.memValidateAccess(MemArea_P, _offset, true))
		{
			_wordA = _wordB =  0x00badbad;
			return;			
		}

		if( _offset >= sizeP() )
		{
			LOG_ERR_MEM_READ( _offset );
			assert( 0 && "invalid memory address" );
			_wordA = _wordB =  0x00badbad;
			return;
		}
#endif

		_wordA = p[_offset];
		_wordB = p[_offset+1];

#ifdef _DEBUG
		if( _wordA == g_initPattern || _wordB == g_initPattern)
			LOG_ERR_MEM_READ_UNINITIALIZED(MemArea_P,_offset);
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
	
	bool Memory::save(const char* _file, EMemArea _area) const
	{
		std::ofstream out(_file, std::ios::binary | std::ios::trunc);

		if(!out.is_open())
			return false;

		std::vector<uint8_t> buf;
		buf.resize(size(_area) * 3);

		size_t index = 0;

		for(uint32_t i=0; i<size(_area); ++i)
		{
			const auto w = get(_area, i);

			buf[index++] =(w>>16) & 0xff;
			buf[index++] =(w>>8) & 0xff;
			buf[index++] =(w) & 0xff;
		}

		out.write(reinterpret_cast<const char*>(&buf.front()), size(_area) * 3);

		out.close();

		return true;
	}

	bool Memory::saveAssembly(const char* _file, TWord _offset, const TWord _count, bool _skipNops, bool _skipDC, IPeripherals* _peripheralsX, IPeripherals* _peripheralsY) const
	{
		std::ofstream out(_file, std::ios::trunc);

		if(!out.is_open())
			return false;

		const Opcodes opcodes;
		Disassembler disasm(opcodes);

		disasm.addSymbols(*this);
		
		if(_peripheralsX)
			_peripheralsX->setSymbols(disasm);

		if (_peripheralsY)
			_peripheralsY->setSymbols(disasm);

		std::vector<TWord> temp;
		temp.resize(sizeP());
		for(TWord i=_offset; i<_offset + _count; ++i)
			temp[i] = p[i];

		std::string output;
		disasm.disassembleMemoryBlock(output, temp, _offset, _skipNops, true, true);
		out << output;
		out.close();
		return true;
	}

	bool Memory::saveAsText(const char* _file, EMemArea _area, const TWord _offset, const TWord _count) const
	{
		std::ofstream out(_file, std::ios::trunc);
		if(!out.is_open())
			return false;

		auto putVal = [&](TWord v)
		{
			char temp[16];
			sprintf(temp, "%06x", v);
			return std::string(temp);
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

				if(symbol.names.find(_name) == symbol.names.end())
				{
					symbol.names.insert(_name);
				}
			}
		}
	}

	const std::string& Memory::getSymbol(EMemArea _memArea, const TWord addr) const
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
			for(size_t i=0; i<size(static_cast<EMemArea>(a)); ++i)
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
