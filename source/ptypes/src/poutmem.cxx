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

#include "pstreams.h"


namespace ptypes {


outmemory::outmemory(int ilimit, int iincrement)
    : outstm(false, 0), mem(), limit(ilimit), increment(iincrement)
{
}


outmemory::~outmemory()
{
    close();
}


int outmemory::classid()
{
    return CLASS2_OUTMEMORY;
}


void outmemory::doopen()
{
}


void outmemory::doclose()
{
    clear(mem);
}


int outmemory::doseek(int newpos, ioseekmode mode)
{
    int pos;

    switch (mode)
    {
    case IO_BEGIN:
        pos = newpos;
        break;
    case IO_CURRENT:
        pos = abspos + newpos;
        break;
    default: // case IO_END:
        pos = length(mem) + newpos;
        break;
    }

    if (pos > limit)
        pos = limit;
    
    return pos;
}


int outmemory::dorawwrite(const char* buf, int count)
{
    if (count <= 0)
        return 0;
    if (limit > 0 && abspos + count > limit)
    {
        count = limit - abspos;
        if (count <= 0)
            return 0;
    }

    // the string reallocator takes care of efficiency
    if (abspos + count > length(mem))
        setlength(mem, abspos + count);
    memcpy(pchar(pconst(mem)) + abspos, buf, count);
    return count;
}


string outmemory::get_streamname() 
{
    return "mem";
}


string outmemory::get_strdata()
{
    if (!active)
        errstminactive();
    return mem;
}


}
