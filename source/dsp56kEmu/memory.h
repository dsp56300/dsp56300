#pragma once

#include <array>
#include <map>
#include <set>
#include <vector>

#include "peripherals.h"

namespace dsp56k
{
	// 128 words - boostrap extension
	#define MemArea_P_Motorola_Reserved_Begin	0xFF8780
	#define MemArea_P_Motorola_Reserved_End		0xFF87FF

	// 192 words
	#define MemArea_P_Bootstrap_Begin			0xFF0000
	#define MemArea_P_Bootstrap_End				0xFFF0bF

	#define MemArea_P_Reserved0_Begin			0xFF00C0
	#define MemArea_P_Reserved0_End				0xFF0FFF

	#define MemArea_P_Reserved1_Begin			0xFF8800
	#define MemArea_P_Reserved1_End				0xFFFFFF

	#define MemArea_X_IOInternal_Begin			0xFFFF80
	#define MemArea_X_IOInternal_End			0xFFFFFF

	class DSP;

	class IMemoryValidator
	{
	public:
		virtual ~IMemoryValidator() = default;
		virtual bool memValidateAccess	( EMemArea _area, TWord _addr, bool _write ) const = 0;
	};

	class DefaultMemoryMap final : public IMemoryValidator
	{
	public:
		bool memValidateAccess(EMemArea _area, TWord _addr, bool _write) const override	{ return true; }
	};

	class Memory final
	{
		// _____________________________________________________________________________
		// members
		//

		const IMemoryValidator&									m_memoryMap;
		
		// 768k words of 24-bit data for 3 banks (XYP)
		const size_t										m_size;
		std::vector<TWord>									m_buffer;
		StaticArray< TWord*, MemArea_COUNT >				m_mem;

		TWord*												x;
		TWord*												y;
		TWord*												p;

		TWord												m_bridgedMemoryAddress = 0xffffff;
		
		struct STransaction
		{
			unsigned int ictr;
			EMemArea area;
			TWord offset;
			TWord oldVal;
			TWord newVal;
		};

		struct SSymbol
		{
			std::set<std::string> names;
			TWord address;
			char area;
		};

		DSP*						m_dsp;
		std::vector<STransaction>	m_transactionHistory;

		std::map<char, std::map<TWord, SSymbol>> m_symbols;

		// _____________________________________________________________________________
		// implementation
		//
	public:
		Memory(const IMemoryValidator& _memoryMap, size_t _memSize = 0xc00000, TWord* _externalBuffer = nullptr);
		Memory(const Memory&) = delete;
		Memory& operator = (const Memory&) = delete;

		bool				loadOMF				( const std::string& _filename );

		bool				set					( EMemArea _area, TWord _offset, TWord _value )	{ return dspWrite(_area, _offset, _value); }

		bool				dspWrite			( EMemArea& _area, TWord& _offset, TWord _value );
		TWord				get					( EMemArea _area, TWord _offset ) const;
		void				get2				( EMemArea _area, TWord _offset, TWord& _wordA, TWord& _wordB);

		bool				save				( FILE* _file ) const;
		bool				load				( FILE* _file );

		bool				save				(const char* _file, EMemArea _area);
		bool				saveAssembly		(const char* _file, TWord _offset, const TWord _count, bool _skipNops = true, bool _skipDC = false);

		bool				saveAsText			(const char* _file, EMemArea _area, const TWord _offset, const TWord _count);

		void				setDSP				( DSP* _dsp )	{ m_dsp = _dsp; }

		void				setSymbol			(char _area, TWord _address, const std::string& _name);
		const std::string&	getSymbol			(EMemArea _memArea, TWord addr);
		const std::map<char, std::map<TWord, SSymbol>>& getSymbols() const { return m_symbols; }

		size_t				size				() const	{ return m_size; }

		void				setExternalMemory	(const TWord _address, bool _isExternalMemoryBridged)
		{
			m_bridgedMemoryAddress = _isExternalMemoryBridged ? _address : 0;
		}

	private:
		void				fillWithInitPattern	();
		void				memTranslateAddress	(EMemArea& _area, TWord& _addr) const;
	};
}
