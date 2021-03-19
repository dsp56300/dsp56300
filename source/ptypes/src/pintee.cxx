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


intee::intee(instm* istm, const char* ifn, bool iappend)
    : infilter(istm, -1), file(ifn, iappend)  
{
}


intee::intee(instm* istm, const string& ifn, bool iappend)
    : infilter(istm, -1), file(ifn, iappend)  
{
}


intee::~intee() 
{
    close();
}


void intee::doopen() 
{
    infilter::doopen();
    file.open();
}


void intee::doclose() 
{
    file.close();
    infilter::doclose();
}


void intee::dofilter() 
{
    int count = stm->read(savebuf, savecount);
    if (count > 0) 
    {
        file.write(savebuf, count);
        savebuf += count;
        savecount -= count;
    }
}


string intee::get_streamname() 
{
    return "tee: " + file.get_filename();
}


}
