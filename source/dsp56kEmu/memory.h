#pragma once

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

	class Jitmem;

	class IMemoryValidator
	{
	public:
		virtual ~IMemoryValidator() = default;
		virtual bool memValidateAccess	( EMemArea _area, TWord _addr, bool _write ) const = 0;
	};

	class DefaultMemoryValidator final : public IMemoryValidator
	{
	public:
		bool memValidateAccess(EMemArea _area, TWord _addr, bool _write) const override	{ return true; }
	};

	class Memory final
	{
		friend class Jitmem;

		// _____________________________________________________________________________
		// members
		//

		const IMemoryValidator&								m_memoryMap;
		
		// number of words of 24-bit data for 3 banks (XYP)
		std::array<TWord, MemArea_COUNT>					m_size;
		std::vector<TWord>									m_buffer;
		std::array<TWord*, MemArea_COUNT>					m_mem;

		TWord*												x;
		TWord*												y;
		TWord*												p;

		TWord												m_bridgedMemoryAddress;
		
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
		explicit Memory(const IMemoryValidator& _memoryMap, TWord _memSize = 0xc00000, TWord* _externalBuffer = nullptr);
		explicit Memory(const IMemoryValidator& _memoryMap, TWord _memSizeP, TWord _memSizeXY, TWord _brigedMemoryAddress, TWord* _externalBuffer = nullptr);
		Memory(const Memory&) = delete;
		Memory& operator = (const Memory&) = delete;

		bool				loadOMF				( const std::string& _filename );

		bool				set					( EMemArea _area, TWord _offset, TWord _value )	{ return dspWrite(_area, _offset, _value); }

		bool				dspWrite			( EMemArea& _area, TWord& _offset, TWord _value );
		TWord				get					( EMemArea _area, TWord _offset ) const;
		void				getOpcode			( TWord _offset, TWord& _wordA, TWord& _wordB ) const;

		bool				save				(const char* _file, EMemArea _area) const;
		bool				saveAssembly		(const char* _file, TWord _offset, const TWord _count, bool _skipNops = true, bool _skipDC = false, IPeripherals* _peripheralsX = nullptr, IPeripherals* _peripheralsY = nullptr) const;

		bool				saveAsText			(const char* _file, EMemArea _area, const TWord _offset, const TWord _count) const;

		void				setDSP				( DSP* _dsp )	{ m_dsp = _dsp; }

		void				setSymbol			(char _area, TWord _address, const std::string& _name);
		const std::string&	getSymbol			(EMemArea _memArea, TWord addr) const;
		const std::map<char, std::map<TWord, SSymbol>>& getSymbols() const { return m_symbols; }

		TWord				size				(EMemArea _area) const	{ return m_size[_area]; }
		TWord				sizeXY				() const				{ return size(MemArea_X); }
		TWord				sizeP				() const				{ return size(MemArea_P); }

		void				setExternalMemory	(const TWord _address, bool _isExternalMemoryBridged)
		{
			m_bridgedMemoryAddress = _isExternalMemoryBridged ? _address : 0;
		}

		const TWord&		getBridgedMemoryAddress() const { return m_bridgedMemoryAddress; }

		TWord*				getMemAreaPtr		(EMemArea _area)
		{
			switch (_area)
			{
			case MemArea_P: return p;
			case MemArea_X: return x;
			case MemArea_Y: return y;
			default:		return nullptr;
			}
		}

		// As XY is bridged to P for all addresses >= _brigedMemoryAddress, we need to allocate more for P but less for XY if a bridged address is specified
		static constexpr TWord calcXYMemSize(TWord _memSizeXY, TWord _bridgedMemoryAddress)
		{
			return _bridgedMemoryAddress ? _bridgedMemoryAddress : _memSizeXY;
		}

		static constexpr TWord calcPMemSize(TWord _memSizeP, TWord _memSizeXY, TWord _bridgedMemoryAddress)
		{
			return _bridgedMemoryAddress ? std::max(_memSizeXY, _memSizeP) : _memSizeP;
		}

		static constexpr TWord calcMemSize(TWord _memSizeP, TWord _memSizeXY, TWord _bridgedMemoryAddress)
		{
			return calcPMemSize(_memSizeP, _memSizeXY, _bridgedMemoryAddress) + 2 * calcXYMemSize(_memSizeXY, _bridgedMemoryAddress);
		}
		
		static constexpr TWord calcMemSize(TWord _memSize, TWord _bridgedMemoryAddress)
		{
			return calcPMemSize(_memSize, _memSize, _bridgedMemoryAddress) + 2 * calcXYMemSize(_memSize, _bridgedMemoryAddress);
		}

	private:
		void				fillWithInitPattern	();
		void				memTranslateAddress	(EMemArea& _area, TWord& _addr) const;
	};
}
