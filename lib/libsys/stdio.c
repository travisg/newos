/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/printf.h>
#include <libc/string.h>
#include <libsys/syscalls.h>

//static int infd = -1;
//static int outfd = -1;

int __stdio_init()
{
	return 0;
}

int __stdio_deinit()
{
	return 0;
}

int printf(const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);
	
	sys_write(0, buf, 0, strlen(buf));
	
	return i;
}

char getc()
{
	char c;
	int len;
	
	sys_read(1, &c, 0, 1);
	return c;
}
