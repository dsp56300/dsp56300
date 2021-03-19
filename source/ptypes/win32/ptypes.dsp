# Microsoft Developer Studio Project File - Name="ptypes" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=ptypes - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ptypes.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ptypes.mak" CFG="ptypes - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ptypes - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "ptypes - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Perforce Project"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ptypes - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../lib"
# PROP Intermediate_Dir "../../temp/Release/ptypes"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /GX /O2 /I "../include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "_WIN32" /D "__WIN32__" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "ptypes - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../lib"
# PROP Intermediate_Dir "../../temp/Debug/ptypes"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "_WIN32" /D "__WIN32__" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../lib\ptypesD.lib"

!ENDIF 

# Begin Target

# Name "ptypes - Win32 Release"
# Name "ptypes - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;h"
# Begin Source File

SOURCE=..\src\pasync.cxx
# End Source File
# Begin Source File

SOURCE=..\src\patomic.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pcomponent.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pcset.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pcsetdbg.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pexcept.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pfatal.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pfdxstm.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pinfile.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pinfilter.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pinmem.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pinstm.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pintee.cxx
# End Source File
# Begin Source File

SOURCE=..\src\piobase.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pipbase.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pipmsg.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pipmsgsv.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pipstm.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pipstmsv.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pipsvbase.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pmd5.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pmem.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pmsgq.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pmtxtable.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pnpipe.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pnpserver.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pobjlist.cxx
# End Source File
# Begin Source File

SOURCE=..\src\poutfile.cxx
# End Source File
# Begin Source File

SOURCE=..\src\poutfilter.cxx
# End Source File
# Begin Source File

SOURCE=..\src\poutmem.cxx
# End Source File
# Begin Source File

SOURCE=..\src\poutstm.cxx
# End Source File
# Begin Source File

SOURCE=..\src\ppipe.cxx
# End Source File
# Begin Source File

SOURCE=..\src\ppodlist.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pputf.cxx
# End Source File
# Begin Source File

SOURCE=..\src\prwlock.cxx
# End Source File
# Begin Source File

SOURCE=..\src\psemaphore.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstdio.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstrcase.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstrconv.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstring.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstrlist.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstrmanip.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstrtoi.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pstrutils.cxx
# End Source File
# Begin Source File

SOURCE=..\src\ptextmap.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pthread.cxx
# End Source File
# Begin Source File

SOURCE=..\src\ptime.cxx
# End Source File
# Begin Source File

SOURCE=..\src\ptimedsem.cxx
# End Source File
# Begin Source File

SOURCE=..\src\ptrigger.cxx
# End Source File
# Begin Source File

SOURCE=..\src\punit.cxx
# End Source File
# Begin Source File

SOURCE=..\src\punknown.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pvariant.cxx
# End Source File
# Begin Source File

SOURCE=..\src\pversion.cxx
# End Source File
# End Group
# Begin Group "Include"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\pasync.h
# End Source File
# Begin Source File

SOURCE=..\include\pinet.h
# End Source File
# Begin Source File

SOURCE=..\include\pport.h
# End Source File
# Begin Source File

SOURCE=..\include\pstreams.h
# End Source File
# Begin Source File

SOURCE=..\include\ptime.h
# End Source File
# Begin Source File

SOURCE=..\include\ptypes.h
# End Source File
# End Group
# Begin Group "Makefile"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Makefile
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.BSD_OS
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.common
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.CYGWIN_NT
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.Darwin
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.FreeBSD
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.Linux
# End Source File
# Begin Source File

SOURCE=..\src\Makefile.SunOS
# End Source File
# End Group
# End Target
# End Project
