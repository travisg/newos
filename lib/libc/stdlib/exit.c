/*
** Copyright 2002-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#if !_KERNEL
#include <stdlib.h>
#include <sys/syscalls.h>

void exit(int code)
{
    // XXX fix me
    _kern_exit(code);
}

void _exit(int code)
{
    _kern_exit(code);
}

void _Exit(int code)
{
    _kern_exit(code);
}

#endif

