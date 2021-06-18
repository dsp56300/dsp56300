#pragma once

#include "error.h"
#include "types.h"
#include "utils.h"

namespace dsp56k
{
	class AGU
	{
	public:
		static TWord calcModuloMask(TWord m)
		{
//			const auto zeroBits = countZeroBitsReversed(m);
//			const int testRes = (1<<((sizeof(m)*CHAR_BIT)-zeroBits))-1;

			m |= m >> 1;
			m |= m >> 2;
			m |= m >> 4;
			m |= m >> 8;

//			assert(m == testRes);
			return m;
		}

		static void updateAddressRegister( TWord& r, int n, TWord m, TWord moduloMask, int32_t modulo )
		{
			// modulo or linear addressing
			if (moduloMask)
			{
				n=signextend<int,24>(n);
				if( abs(n) >= modulo )	// linear addressing OR modulo with increment exceeding modulo.
				{
					// the doc says it's only valid for N = P x (2 pow k), but assume the assembly is okay
//						LOG( "r " << std::hex << r << " + n " << std::hex << n << " = " << std::hex << ((r+n)&0x00ffffff) );
					r += n;
					r &= 0x00ffffff;
				}
				else
				{
					// If (rOffset<0) then bit 31 of rOffset is set. Shift it down 31 places to get -1. Otherwise you have 0.
					// and this value with modulo to get either modulo or zero. Add to rOffset.
					// If (moduloTest-rOffset<0) (rOffset>moduloTest) (i.e. rOffset exceeds moduloTest), do the same trick.
					const int32_t p				= (r&moduloMask) + n;
					const int32_t mt			= m - p;
					r							+= n + ((p>>31) & modulo) - (((mt)>>31) & modulo);
				}
			}
			else	// bit-reverse mode
			{
				assert( 0 && "bitreverse mode is used" );

				r = bitreverse24(r);
				n = bitreverse24(n);
				r += n;
				r &= 0x00ffffff;
				r = bitreverse24(r);
			}
		}
	};
};
