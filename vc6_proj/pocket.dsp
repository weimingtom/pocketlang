# Microsoft Developer Studio Project File - Name="pocket" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=pocket - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pocket.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pocket.mak" CFG="pocket - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pocket - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "pocket - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pocket - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x804 /d "NDEBUG"
# ADD RSC /l 0x804 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "pocket - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "." /I "..\src\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "pocket - Win32 Release"
# Name "pocket - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\cli\main.c
# End Source File
# Begin Source File

SOURCE=..\cli\modules.c

!IF  "$(CFG)" == "pocket - Win32 Release"

!ELSEIF  "$(CFG)" == "pocket - Win32 Debug"

# SUBTRACT CPP /X

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\pk_compiler.c
# End Source File
# Begin Source File

SOURCE=..\src\pk_core.c
# End Source File
# Begin Source File

SOURCE=..\src\pk_debug.c
# End Source File
# Begin Source File

SOURCE=..\src\pk_utils.c
# End Source File
# Begin Source File

SOURCE=..\src\pk_var.c
# End Source File
# Begin Source File

SOURCE=..\src\pk_vm.c
# End Source File
# Begin Source File

SOURCE=..\cli\repl.c

!IF  "$(CFG)" == "pocket - Win32 Release"

!ELSEIF  "$(CFG)" == "pocket - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\cli\thirdparty.c

!IF  "$(CFG)" == "pocket - Win32 Release"

!ELSEIF  "$(CFG)" == "pocket - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\cli\utils.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\cli\common.h
# End Source File
# Begin Source File

SOURCE=..\cli\internal.h
# End Source File
# Begin Source File

SOURCE=.\inttypes.h
# End Source File
# Begin Source File

SOURCE=..\cli\modules.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_buffers.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_common.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_compiler.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_core.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_debug.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_internal.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_opcodes.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_utils.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_var.h
# End Source File
# Begin Source File

SOURCE=..\src\pk_vm.h
# End Source File
# Begin Source File

SOURCE=..\src\include\pocketlang.h
# End Source File
# Begin Source File

SOURCE=.\stdbool.h
# End Source File
# Begin Source File

SOURCE=.\stdint.h
# End Source File
# Begin Source File

SOURCE=..\cli\utils.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
