/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>

#include <kernel/arch/dbg_console.h>

int arch_dbg_con_init(kernel_args *ka)
{
	return 0;
}

char arch_dbg_con_read()
{
	return 0;
}

char arch_dbg_con_putch(const char c)
{
	return c;
}

void arch_dbg_con_puts(const char *s)
{
}

ssize_t arch_dbg_con_write(const void *buf, ssize_t len)
{
	return 0;
}

