/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <boot/stage2.h>

int con_init(kernel_args *ka);
int kprintf(const char *fmt, ...);
int kprintf_xy(int x, int y, const char *fmt, ...);

#endif
