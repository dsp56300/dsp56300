#pragma once

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#endif

#include <string>
#include <vector>
#include <list>

#include "logging.h"

#include "assert.h"			// our own implementation

#include "staticArray.h"	// useful helper class

namespace dsp56k
{
	// our fingers like them
	typedef unsigned int uint;
	typedef unsigned char uchar;
	typedef unsigned short ushort;
}
