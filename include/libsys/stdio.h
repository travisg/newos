/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _STDIO_H
#define _STDIO_H

#include <sys/cdefs.h>
#include <libc/stdarg.h>

int printf(const char *fmt, ...) __PRINTFLIKE(1,2);
char getc(void);

#endif

