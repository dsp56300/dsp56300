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
#  include <pthread.h>
#endif

#include "pasync.h"


namespace ptypes {


#ifndef __SEM_TO_TIMEDSEM__


static void sem_fail()
{
    fatal(CRIT_FIRST + 41, "Semaphore failed");
}


semaphore::semaphore(int initvalue) 
{
    if (sem_init(&handle, 0, initvalue) != 0)
        sem_fail();
}


semaphore::~semaphore() 
{
    sem_destroy(&handle);
}


void semaphore::wait() 
{
    if (sem_wait(&handle) != 0)
        sem_fail();
}


void semaphore::post() 
{
    if (sem_post(&handle) != 0)
        sem_fail();
}


#else


int _psemaphore_dummy_symbol;  // avoid ranlib's warning message


#endif



}
