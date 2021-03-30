#pragma once

#include <array>

#include "peripherals.h"
#include "registers.h"

namespace dsp56k
{
	class DSP;
	class Memory;

	class Device
	{
		// _____________________________________________________________________________
		// members
		//
	private:
		std::array<PeripheralsDefault,2>	m_peripherals;
		DSP*								m_dsp;
		Memory*								m_memory;
		TWord		m_execAddr;

		// _____________________________________________________________________________
		// implementation
		//
	public:
		Device						();
		virtual ~Device				();

		bool loadOMF				( const char* _filename );

		bool readReg				( EReg _reg, TReg24& _dst ) const;
		bool writeReg				( EReg _reg, TReg24 _src );

		bool readReg				( EReg _reg, TReg56& _dst ) const;
		bool writeReg				( EReg _reg, const TReg56& _src );

		bool readMem				( EMemArea _area, unsigned int _offset, TWord& _dst );
		bool writeMem				( EMemArea _area, unsigned int _offset, TWord _src );
		bool fillMem				( EMemArea _area, unsigned int _offset, unsigned long _size, TWord _val );

		void dumpRegs				();

		bool saveState				( const char* _filename );
		bool loadState				( const char* _filename );

		bool saveState				( FILE* _file );
		bool loadState				( FILE* _file );

		void exec					();
		void execUntilRTS			();

		bool showAssembly			( unsigned long _addr, char* _result ); 
		bool assemble				( char* _str, unsigned long* _opcodes, char* _errorPtr );

		bool setExecutionAddress	( unsigned long _addr );

		void readDebugRegs			( SRegs& _regs );

		void dumpAssembly			( const char* _outfile );
	};
};