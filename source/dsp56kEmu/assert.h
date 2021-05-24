#pragma once

namespace dsp56k
{
	class Assert
	{
	public:
		static void show( const char* _msg );
	};
}

#ifdef _WIN32
#ifdef assert
#	undef assert
#endif

#ifdef _DEBUG
#define assert( expr )			{ if( !(expr) ) dsp56k::Assert::show( #expr ); }
#define assertf( expr, msg )	{ if( !(expr) ) dsp56k::Assert::show( msg ); }
#else
#define assert( expr )			{}
#define assertf( expr, msg )	{}
#endif
#else
#include <cassert>
#	define assertf( expr, msg )	assert( (expr) && #msg )
#endif
