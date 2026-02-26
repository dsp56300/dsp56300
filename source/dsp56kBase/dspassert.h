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
#define assert( expr )			do { if( !(expr) ) dsp56k::Assert::show( #expr, __func__, __LINE__ ); } while(0)
#define assertf( expr, msg )	do { if( !(expr) ) dsp56k::Assert::show( msg, __func__, __LINE__ ); } while(0)
#else
#define assert( expr )			do {} while(0)
#define assertf( expr, msg )	do {} while(0)
#endif
