#include "pch.h"

#include "device.h"
#include "memory.h"

#include "dsp.h"
#include "memory.h"
#include "OMFLoader.h"

namespace dsp56k
{
	// _____________________________________________________________________________
	// Device
	//
	Device::Device() : m_dsp(0), m_memory(0), m_execAddr(0)
	{
		m_memory	= new dsp56k::Memory();
		m_dsp		= new dsp56k::DSP(*m_memory);
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
	bool Device::readReg( EReg _reg, TReg24& _dst )
	{
		return m_dsp->readReg( _reg, _dst );
	}
	// _____________________________________________________________________________
	// readReg
	//
	bool Device::readReg( EReg _reg, TReg56& _dst )
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
		return false;
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
//	bool Device::loadState( const char* _filename )
//	{
//		dsp_load( _filename, m_id, m_id );
//	}
	// _____________________________________________________________________________
	// reset
	//
// 	void Device::reset()
// 	{
//		dsp_init( m_id );
//	}
};
