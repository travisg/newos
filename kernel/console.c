/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char bootup_buffer[_BOOT_KPRINTF_BUF_SIZE];
static volatile int head;
static volatile int tail;

static int bootup_kputs(const char *str);

static kernel_console_ops bootup_kops = {
	&bootup_kputs,
	NULL, // x y ops unsupported until full console up
	NULL
};

static kernel_console_ops *ops = &bootup_kops;

// puts into a circular buffer
static int bootup_kputs(const char *str)
{
	int len;
	int pos = 0;

	if(!str)
		return 0;

	len = strlen(str);
	while(pos < len) {
		if(head < (_BOOT_KPRINTF_BUF_SIZE-1)) 
			bootup_buffer[head++] = str[pos];
		pos++;
	}

	return len;
}

int kprintf(const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	char temp[256];

	if(ops && ops->kputs) {
		va_start(args, fmt);
		ret = vsprintf(temp,fmt,args);
		va_end(args);

		ops->kputs(temp);
	}
	return ret;
}

int kprintf_xy(int x, int y, const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	char temp[256];

	if(ops && ops->kputs_xy) {
		va_start(args, fmt);
		ret = vsprintf(temp,fmt,args);
		va_end(args);

		ops->kputs_xy(temp, x, y);
	}
	return ret;
}

int kputchar_xy(char c, int x, int y)
{
	if(ops && ops->kputchar_xy)
		return ops->kputchar_xy(c, x, y);
	else
		return 0;
}

void con_register_ops(kernel_console_ops *_ops)
{
	kernel_console_ops *old_ops = ops;

	ops = _ops;

	if(old_ops == &bootup_kops) {
		// XXX find better way
		if(tail < head) {
			bootup_buffer[head] = 0;
			ops->kputs(&bootup_buffer[tail]);
		}
	}
}

int con_init(kernel_args *ka)
{
	dprintf("con_init: entry\n");

	return 0;
}
