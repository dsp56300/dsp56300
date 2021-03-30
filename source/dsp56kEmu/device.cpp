#include "pch.h"

#include "device.h"
#include "memory.h"

#include "dsp.h"
#include "memory.h"
#include "omfloader.h"

namespace dsp56k
{
	// _____________________________________________________________________________
	// Device
	//
	Device::Device() : m_dsp(0), m_memory(0), m_execAddr(0)
	{
		m_memory	= new Memory(&m_peripherals[0], &m_peripherals[1]);
		m_dsp		= new DSP(*m_memory);
	}
	// _____________________________________________________________________________
	// ~Device
	//
	Device::~Device()
	{
		delete m_dsp;
		delete m_memory;
	}

	// _____________________________________________________________________________
	// loadOMF
	//
	bool Device::loadOMF( const char* _filename )
	{
		dsp56k::OMFLoader loader;
		return loader.load( _filename, *m_memory );
	}

	// _____________________________________________________________________________
	// readReg
	//
	bool Device::readReg( EReg _reg, TReg24& _dst ) const
	{
		return m_dsp->readReg( _reg, _dst );
	}
	// _____________________________________________________________________________
	// readReg
	//
	bool Device::readReg( EReg _reg, TReg56& _dst ) const
	{
		return m_dsp->readReg( _reg, _dst );
	}
	// _____________________________________________________________________________
	// writeReg
	//
	bool Device::writeReg( EReg _reg, TReg24 _src )
	{
		return m_dsp->writeReg( _reg, _src );
	}
	// _____________________________________________________________________________
	// writeReg
	//
	bool Device::writeReg( EReg _reg, const TReg56& _src )
	{
		return m_dsp->writeReg( _reg, _src );
	}
	// _____________________________________________________________________________
	// dumpRegs
	//
	void Device::dumpRegs()
	{
	}
	// _____________________________________________________________________________
	// readMem
	//
	bool Device::readMem( EMemArea _area, unsigned int _offset, TWord& _dst )
	{
		_dst = m_memory->get( _area, _offset );
		return true;
	}
	// _____________________________________________________________________________
	// writeMem
	//
	bool Device::writeMem( EMemArea _area, unsigned int _offset, TWord _src )
	{
		return m_memory->set( _area, _offset, _src );
	}
	// _____________________________________________________________________________
	// fillMem
	//
	bool Device::fillMem( EMemArea _area, unsigned int _offset, unsigned long _size, TWord _val )
	{
		for( TWord i=_offset; i<_offset+_size; ++i )
		{
			if( !m_memory->set( _area, i, _val ) )
				return false;
		}
		return true;
	}
	// _____________________________________________________________________________
	// saveState
	//
	bool Device::saveState( const char* _filename )
	{
		FILE* hFile = fopen( _filename, "wb" );
		if( !hFile )
		{
			LOG( "Failed to save state to file " << _filename );
			return false;
		}
		const bool ret = saveState( hFile );
		fclose(hFile);
		return ret;
	}

	// _____________________________________________________________________________
	// saveState
	//
	bool Device::saveState( FILE* _file )
	{
		if( !m_memory->save(_file) )
		{
			LOG( "Failed to dump memory to file" );
			return false;
		}
		if( !m_dsp->save(_file) )
		{
			LOG( "Failed to dump DSP to file" );
		}
		return true;
	}

	// _____________________________________________________________________________
	// exec
	//
	void Device::exec()
	{
		m_dsp->exec();
	}

	// _____________________________________________________________________________
	// execUntilRTS
	//
	void Device::execUntilRTS()
	{
		m_dsp->jsr( TReg24((int)m_execAddr) );
//		m_dsp->logSC("jsr ea START");
		m_dsp->execUntilRTS();
	}

	// _____________________________________________________________________________
	// showAssembly
	//
	bool Device::showAssembly( unsigned long _addr, char* _result )
	{
		return false;
	}
	// _____________________________________________________________________________
	// assemble
	//
	bool Device::assemble( char* _str, unsigned long* _opcodes, char* _errorPtr )
	{
		return false;
	}
	// _____________________________________________________________________________
	// jumpToAddress
	//
	bool Device::setExecutionAddress( unsigned long _addr )
	{
		m_execAddr = _addr;
		return true;
	}
	// _____________________________________________________________________________
	// readDebugRegs
	//
	void Device::readDebugRegs( SRegs& _regs )
	{
		m_dsp->readDebugRegs( _regs );
	}

	// _____________________________________________________________________________
	// dumpAssembly
	//
	void Device::dumpAssembly( const char* _outfile )
	{
	}

	// _____________________________________________________________________________
	// loadState
	//
	bool Device::loadState( const char* _filename )
	{
		FILE* hFile = fopen(_filename, "rb" );
		if( !hFile )
			return false;

		loadState(hFile);
		fclose(hFile);

		return true;
	}

	// _____________________________________________________________________________
	// loadState
	//
	bool Device::loadState( FILE* _file )
	{
		if( !m_memory->load(_file) )
		{
			LOG( "Failed to dump memory to file" );
			return false;
		}
		if( !m_dsp->load(_file) )
		{
			LOG( "Failed to dump DSP to file" );
		}
		return true;
	}

	// _____________________________________________________________________________
	// reset
	//
// 	void Device::reset()
// 	{
//		dsp_init( m_id );
//	}
};
