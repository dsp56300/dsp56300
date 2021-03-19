/*
 *
 *  C++ Portable Types Library (PTypes)
 *  Version 2.0.2  Released 17-May-2004
 *
 *  Copyright (C) 2001-2004 Hovik Melikyan
 *
 *  http://www.melikyan.com/ptypes/
 *
 */


#include "pasync.h"


namespace ptypes {


pmemlock _mtxtable[_MUTEX_HASH_SIZE]
#ifndef WIN32
    = {
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT, _MTX_INIT,
    _MTX_INIT
}
#endif
;


}
