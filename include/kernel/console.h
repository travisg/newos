/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <sys/cdefs.h>
#include <boot/stage2.h>

int con_init(kernel_args *ka);
int kprintf(const char *fmt, ...) __PRINTFLIKE(1,2);
int kprintf_xy(int x, int y, const char *fmt, ...) __PRINTFLIKE(3,4);

#endif
