/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>

#include <libc/stdarg.h>
#include <libc/printf.h>

// from con.h
enum {
	CONSOLE_OP_WRITEXY = 2376
};

struct console_op_xy_struct {
	int x;
	int y;
	char buf[256];
};

static int console_fd = -1;

int kprintf(const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	char temp[256];

	if(console_fd >= 0) {
		va_start(args, fmt);
		ret = vsprintf(temp,fmt,args);
		va_end(args);
	
		sys_write(console_fd, temp, 0, &ret);
	}
	return ret;
}

int kprintf_xy(int x, int y, const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	struct console_op_xy_struct buf;

	if(console_fd >= 0) {
		va_start(args, fmt);
		ret = vsprintf(buf.buf,fmt,args);
		va_end(args);
	
		buf.x = x;
		buf.y = y;
		sys_ioctl(console_fd, CONSOLE_OP_WRITEXY, &buf, ret + sizeof(buf.x) + sizeof(buf.y));
	}
	return ret;
}

int con_init(kernel_args *ka)
{
	dprintf("con_init: entry\n");

	console_fd = sys_open("/dev/console", "", STREAM_TYPE_DEVICE);
	dprintf("console_fd = %d\n", console_fd);

	return 0;
}
