/* 
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_CONSOLE_H
#define _KERNEL_CONSOLE_H

#include <boot/stage2.h>
#include <sys/cdefs.h>

// the current console driver needs to register back with the kernel with
// these calls so the kernel can dump debug or emergency messages directly
// to the screen (or whatever the console is). These calls need to bypass
// any high level locks and go directly to hardware.
typedef struct {
	int (*kputs)(const char *str);
	int (*kputs_xy)(const char *str, int x, int y);
	int (*kputchar_xy)(char c, int x, int y);
} kernel_console_ops;

void con_register_ops(kernel_console_ops *ops);

int con_init(kernel_args *ka);
int kprintf(const char *fmt, ...) __PRINTFLIKE(1,2);
int kprintf_xy(int x, int y, const char *fmt, ...) __PRINTFLIKE(3,4);
int kputchar_xy(char c, int x, int y);

#endif
