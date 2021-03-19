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

#ifndef WIN32
#  include <stdlib.h>
#  include <unistd.h>
#  include <pthread.h>
#  ifdef __sun__
#    include <poll.h>
#  endif
#endif

#include "pasync.h"


namespace ptypes {


void ptdecl psleep(uint milliseconds)
{
#if defined(WIN32)
    Sleep(milliseconds);
#elif defined(__sun__)
    poll(0, 0, milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}


pthread_id_t ptdecl pthrself() 
{
#ifdef WIN32
    return (int)GetCurrentThreadId();
#else
    return pthread_self();
#endif
}


bool ptdecl pthrequal(pthread_id_t id)
{
#ifdef WIN32
    return GetCurrentThreadId() == (uint)id;
#else
    return pthread_equal(pthread_self(), id);
#endif
}


}
