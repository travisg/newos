/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/printf.h>
#include <libc/string.h>
#include <libsys/stdio.h>
#include <libsys/syscalls.h>

int printf(const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);
	
	sys_write(2, buf, 0, strlen(buf));
	
	return i;
}
