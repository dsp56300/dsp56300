#pragma once

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

	class Memory
	{
		// _____________________________________________________________________________
		// members
		//

		// 768k words of 24-bit data for 3 banks (XYP)
		StaticArray< std::vector<TWord>, MemArea_COUNT >	m_mem;

#ifdef _DEBUG
		const std::vector<TWord>&							x;
		const std::vector<TWord>&							y;
		const std::vector<TWord>&							p;
#endif

		Peripherals											m_perif[2];

		// _____________________________________________________________________________
		// implementation
		//
	public:
		Memory();
		Memory( const Memory& _src );

		virtual ~Memory();

		bool	loadOMF				( const std::string& _filename );

		bool	set					( EMemArea _area, TWord _offset, TWord _value );
		TWord	get					( EMemArea _area, TWord _offset ) const;

		bool	translateAddress	( EMemArea& _area, TWord& _offset ) const;

	private:
		void	fillWithInitPattern();
	};
}
