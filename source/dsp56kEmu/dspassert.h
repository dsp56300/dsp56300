#pragma once

namespace dsp56k
{
	class Assert
	{
	public:
		static void show( const char* _msg, const char* _func, int _line );
	};
}

#ifdef assert
#	undef assert
#endif

#ifdef _DEBUG
#define assert( expr )			{ if( !(expr) ) dsp56k::Assert::show( #expr, __func__, __LINE__ ); }
#define assertf( expr, msg )	{ if( !(expr) ) dsp56k::Assert::show( msg, __func__, __LINE__ ); }
#else
#define assert( expr )			{}
#define assertf( expr, msg )	{}
#endif
