/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _DEBUG_H
#define _DEBUG_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

int dbg_init(kernel_args *ka);
int dbg_init2(kernel_args *ka);
char dbg_putch(char c);
void dbg_puts(const char *s);
bool dbg_set_serial_debug(bool new_val);
int dprintf(const char *fmt, ...);
int panic(const char *fmt, ...);
void kernel_debugger();
int dbg_add_command(void (*func)(int, char **), const char *cmd, const char *desc);

#endif
