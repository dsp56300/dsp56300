#pragma once

#include <stdlib.h>
#include "dspassert.h"

template <typename T, size_t C> class StaticArray
{
	// _____________________________________________________________________________
	// members
	//
private:
	T m_array[C];

	// _____________________________________________________________________________
	// access
	//
public:
	enum { eSize = C };

						StaticArray		()					{}
						StaticArray		( const T& _fill )	{ fill(_fill); }

	inline T&			at				(size_t _i)			{ assert(_i < C && "index out of bounds!");	return m_array[_i]; }
	inline const T&		at				(size_t _i) const	{ assert(_i < C && "index out of bounds!"); return m_array[_i]; }

	inline T&			operator[]		(size_t _i)			{ return at(_i);												}
	inline const T&		operator[]		(size_t _i) const	{ return at(_i);												}

	inline size_t		size			() const			{ return C;														}
	inline size_t		byteSize		() const			{ return sizeof(T) * C;											}

	inline bool			isValidIndex	(size_t _i) const	{ return _i < C;												}

	void				fill			( const T& _val )
	{
		for( size_t i=0; i<C; i++ )
			at(i) = _val;
	}
};
