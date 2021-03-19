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

#ifdef WIN32
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include "pstreams.h"


namespace ptypes {


infile::infile()
    : instm(), filename(), syshandle(invhandle), peerhandle(invhandle)  {}


infile::infile(const char* ifn)
    : instm(), filename(ifn), syshandle(invhandle), peerhandle(invhandle)  {}


infile::infile(const string& ifn)
    : instm(), filename(ifn), syshandle(invhandle), peerhandle(invhandle)  {}


infile::~infile() 
{
    close(); 
}


int infile::classid()
{
    return CLASS2_INFILE;
}


string infile::get_streamname() 
{
    return filename;
}


void infile::doopen() 
{
    if (syshandle != invhandle)
        handle = syshandle;
    else
    {
#ifdef WIN32
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        handle = int(CreateFileA(filename, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 
            FILE_FLAG_SEQUENTIAL_SCAN, 0));
#else
        handle = ::open(filename, O_RDONLY);
#endif
        if (handle == invhandle)
            error(uerrno(), "Couldn't open");
    }
}


void infile::doclose()
{
    instm::doclose();
    syshandle = invhandle;
    peerhandle = invhandle;
}


}
