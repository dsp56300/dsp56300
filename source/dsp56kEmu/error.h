#pragma once

#include "assert.h"
#include "logging.h"

namespace dsp56k
{
}

#define LOG_ERR_BASE(A,ERR)					\
{											\
	std::stringstream ss;	ss << __FUNCTION__ << "@" << __LINE__ << ": " << "DSP 56300 ERROR: " << ERR;		\
	LOGWIN32( ss );							\
	assertf( A, ss.str().c_str() );			\
}

// emulation errors
#define LOG_ERR_NOTIMPLEMENTED(T)					LOG_ERR_BASE( false, "Not implemented: " << T )

// memory errors
#define LOG_ERR_MEM_WRITE(ADDR)						LOG_ERR_BASE( false, "Memory Write: " << HEX(ADDR) )
#define LOG_ERR_MEM_READ(ADDR)						LOG_ERR_BASE( true, "Memory Read: " << HEX(ADDR) )
#define LOG_ERR_MEM_READ_UNINITIALIZED(AREA,ADDR)	LOG_ERR_BASE( true, "Read uninitialized memory: area " << AREA << " @" << HEX(ADDR) )
#define LOG_ERR_DSP_ILLEGALINSTRUCTION(OP)			LOG_ERR_BASE( false, "Illegal instruction: " << HEX(OP) )
