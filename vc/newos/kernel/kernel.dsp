# Microsoft Developer Studio Project File - Name="kernel" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=kernel - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kernel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kernel.mak" CFG="kernel - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kernel - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "kernel - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "kernel"
# PROP Scc_LocalPath "..\..\.."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kernel - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /X /I "../../include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "kernel - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /X /I "../../../include" /D "ARCH_i386" /D "WIN32" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "kernel - Win32 Release"
# Name "kernel - Win32 Debug"
# Begin Group "kernel"

# PROP Default_Filter ""
# Begin Group "vm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm_cache.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm_kernel.mk
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm_page.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm_store_anonymous_noswap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm_store_device.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vm\vm_tests.c
# End Source File
# End Group
# Begin Group "fs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\kernel\fs\bootfs.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\fs\envfs.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\fs\fs_kernel.mk
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\fs\makefile
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\fs\rootfs.c
# End Source File
# End Group
# Begin Group "arch 2"

# PROP Default_Filter ""
# Begin Group "i386"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_cpu.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_dbg_console.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_debug.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_faults.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_i386.S
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_int.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_interrupts.S
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_kernel.mk
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_smp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_thread.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_timer.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_vm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\arch_vm_translation_map.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\kernel.ld
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\arch\i386\makefile
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=..\..\..\kernel\console.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\debug.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\dev.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\elf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\faults.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\heap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\int.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\kernel.mk
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\khash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\lock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\main.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\makefile
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\queue.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\sem.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\smp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\syscalls.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\thread.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\timer.c
# End Source File
# Begin Source File

SOURCE=..\..\..\kernel\vfs.c
# End Source File
# End Group
# Begin Group "include"

# PROP Default_Filter ".h"
# Begin Group "kernel-include"

# PROP Default_Filter ""
# Begin Group "fs-include"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\kernel\fs\bootfs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\fs\envfs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\fs\rootfs.h
# End Source File
# End Group
# Begin Group "arch"

# PROP Default_Filter ""
# Begin Group "i386-include"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\cpu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\faults.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\interrupts.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\kernel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\ktypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\smp_priv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\thread_struct.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\timer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\i386\vm.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\cpu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\dbg_console.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\debug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\faults.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\int.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\kernel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\ktypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\smp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\thread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\thread_struct.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\timer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\vm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\arch\vm_translation_map.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\include\kernel\console.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\debug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\dev.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\elf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\faults.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\faults_priv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\heap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\int.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\kernel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\khash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\ktypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\lock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\queue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\sem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\smp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\smp_priv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\syscalls.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\thread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\timer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vfs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vm_cache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vm_page.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vm_priv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vm_store_anonymous_noswap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\kernel\vm_store_device.h
# End Source File
# End Group
# Begin Group "arch1"

# PROP Default_Filter ""
# Begin Group "i3861"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\arch\i386\types.h
# End Source File
# End Group
# End Group
# Begin Group "boot"

# PROP Default_Filter ""
# Begin Group "arch No. 1"

# PROP Default_Filter ""
# Begin Group "i3862"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\boot\arch\i386\stage2.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\include\boot\arch\stage2.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\include\boot\bootdir.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\boot\stage2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\boot\stage2_struct.h
# End Source File
# End Group
# Begin Group "sys"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\sys\elf32.h
# End Source File
# End Group
# Begin Group "dev"

# PROP Default_Filter ""
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\dev\common\null.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\dev\common\zero.h
# End Source File
# End Group
# Begin Group "arch 3"

# PROP Default_Filter ""
# Begin Group "i386 No. 1"

# PROP Default_Filter ""
# Begin Group "console"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\dev\arch\i386\console\console_dev.h
# End Source File
# End Group
# Begin Group "ide"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\dev\arch\i386\ide\ide_bus.h
# End Source File
# End Group
# Begin Group "pci"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\dev\arch\i386\pci\pci_bus.h
# End Source File
# End Group
# Begin Group "keyboard"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\dev\arch\i386\keyboard\keyboard.h
# End Source File
# End Group
# End Group
# End Group
# Begin Source File

SOURCE=..\..\..\include\dev\devs.h
# End Source File
# End Group
# Begin Group "libc No. 1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\libc\ctype.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\libc\printf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\libc\stdarg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\libc\string.h
# End Source File
# Begin Source File

SOURCE="..\..\..\include\libc\va-alpha.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\include\libc\va-mips.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\include\libc\va-ppc.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\include\libc\va-sh.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\include\libc\va-sparc.h"
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\include\types.h
# End Source File
# End Group
# Begin Group "lib"

# PROP Default_Filter ""
# Begin Group "libc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\lib\libc\atoi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\bcopy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\ctype.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\libc.mk
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\memcmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\memcpy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\memmove.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\memscan.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\memset.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strcat.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strchr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strcmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strcpy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strlen.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strncat.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strncmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strncpy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strnicmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strnlen.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strpbrk.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strrchr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strspn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strstr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\strtok.c
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libc\vsprintf.c
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\lib\lib.mk
# End Source File
# End Group
# End Target
# End Project
