#if !KERNEL
/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

FILE *stdin;
FILE *stdout;
FILE *stderr;

int __stdio_init(void);	 /* keep the compiler happy, these two are not supposed */
int __stdio_deinit(void);/* to be called by anyone except crt0, and crt0 will change soon */

static FILE *__create_FILE_struct(int fd)
{
	FILE *f;

	f = (FILE *)malloc(sizeof(FILE));
	if(!f)
		return f;

	f->fd = fd;

	return f;
}

int __stdio_init(void)
{
	stdin = __create_FILE_struct(0);
	stdout = __create_FILE_struct(1);
	stderr = __create_FILE_struct(2);

	return 0;
}

int __stdio_deinit(void)
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

	sys_write(stdout->fd, buf, -1, strlen(buf));

	return i;
}

int fprintf(FILE *stream, char const *fmt, ...)
{
	va_list args;
	char buf[1024];
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	sys_write(stream->fd, buf, -1, strlen(buf));

	return i;
}

int fflush(FILE *stream)
{
	return 0;
}

int getchar(void)
{
	char c;

	sys_read(stdin->fd, &c, 0, 1);
	return c;
}
#endif
