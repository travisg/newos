/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>

#include <stdarg.h>
#include <stdio.h>

static kernel_console_ops *ops = 0;

int kprintf(const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	char temp[256];

	if(ops) {
		va_start(args, fmt);
		ret = vsprintf(temp,fmt,args);
		va_end(args);

		if(ops)
			ops->kputs(temp);
	}
	return ret;
}

int kprintf_xy(int x, int y, const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	char temp[256];

	if(ops) {
		va_start(args, fmt);
		ret = vsprintf(temp,fmt,args);
		va_end(args);

		if(ops)
			ops->kputs_xy(temp, x, y);
	}
	return ret;
}

int kputchar_xy(char c, int x, int y)
{
	if(ops)
		return ops->kputchar_xy(c, x, y);
	else
		return 0;
}

void con_register_ops(kernel_console_ops *_ops)
{
	ops = _ops;
}

int con_init(kernel_args *ka)
{
	dprintf("con_init: entry\n");

	return 0;
}
