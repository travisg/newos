/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_DBG_CONSOLE
#define _ARCH_DBG_CONSOLE

#include <boot/stage2.h>

char arch_dbg_con_read();
char arch_dbg_con_putch(char c);
void arch_dbg_con_puts(const char *s);
int arch_dbg_con_init(kernel_args *ka);

#endif

