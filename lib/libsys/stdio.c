/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/printf.h>
#include <libc/string.h>
#include <libsys/syscalls.h>

static int infd = -1;
static int outfd = -1;

int __stdio_init()
{
	infd = sys_open("/dev/console", "", STREAM_TYPE_DEVICE);
	if(infd < 0)
		return infd;

	outfd = sys_open("/dev/console", "", STREAM_TYPE_DEVICE);
	if(outfd < 0)
		return outfd;
		
	return 0;
}

int __stdio_deinit()
{
	sys_close(infd);
	sys_close(outfd);
	return 0;
}

int printf(const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	int i;
	size_t len;
	
	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);
	
	len = strlen(buf);
	sys_write(outfd, buf, 0, &len);
	
	return i;
}

char getc()
{
	char c;
	int len;
	
	len = 1;
	sys_read(infd, &c, 0, &len);
	return c;
}
