#
#
#  C++ Portable Types Library (PTypes)
#  Version 2.0.2  Released 17-May-2004
#
#  Copyright (C) 2001-2004 Hovik Melikyan
#
#  http://www.melikyan.com/ptypes/
#
#
#
# Makefile for all Unix platforms, places the library in lib/,
# the shared library in so/. Also builds the test program 
# src/ptypes_test and the sample program bin/wshare.
#

UMAKEFILE=Makefile.`uname|sed 's|/|_|;s|-.*||'`

all:
	cd src ; make -f $(UMAKEFILE) all
	cd wshare ; make -f $(UMAKEFILE) all

clean:
	cd src ; make -f $(UMAKEFILE) clean
	cd wshare ; make -f $(UMAKEFILE) clean

clean-src:
	cd src ; make -f $(UMAKEFILE) clean-src
	cd wshare ; make -f $(UMAKEFILE) clean-src

install:
	cd src ; make -f $(UMAKEFILE) install

uninstall:
	cd src ; make -f $(UMAKEFILE) uninstall

