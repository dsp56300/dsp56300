#pragma once

namespace dsp56k
{
	class Assert
	{
	public:
		static void show( const char* _msg );
	};
}

#define assert( expr )			{ if( !(expr) ) dsp56k::Assert::show( #expr ); }
#define assertf( expr, msg )	{ if( !(expr) ) dsp56k::Assert::show( msg ); }
