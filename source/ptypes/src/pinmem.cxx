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


inmemory::inmemory(const string& imem)
    : instm(length(imem)), mem(imem) 
{
}


inmemory::~inmemory() 
{
    close();
}


int inmemory::classid()
{
    return CLASS2_INMEMORY;
}


void inmemory::bufalloc() 
{
    bufdata = pchar(pconst(mem));
    abspos = bufsize = bufend = length(mem);
}


void inmemory::buffree() 
{
    bufclear();
    bufdata = nil;
}


void inmemory::bufvalidate() 
{
    eof = bufpos >= bufend;
}


void inmemory::doopen() 
{
}


void inmemory::doclose() 
{
}


int inmemory::doseek(int, ioseekmode)
{
    return -1;
}


int inmemory::dorawread(char*, int) 
{
    return 0;
}


string inmemory::get_streamname() 
{
    return "mem";
}


int inmemory::seek(int newpos, ioseekmode mode)
{
    if (mode == IO_END)
    {
        newpos += bufsize;
        mode = IO_BEGIN;
    }
    return instm::seek(newpos, mode);
}


void inmemory::set_strdata(const string& data)
{
    close();
    mem = data;
}


}
