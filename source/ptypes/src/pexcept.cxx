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

#include "ptypes.h"


namespace ptypes {


exception::exception(const char* imsg)
    : message(imsg) 
{
}


exception::exception(const string& imsg) 
    : message(imsg) 
{
}


exception::~exception() 
{
}


}
