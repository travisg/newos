/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/int.h>
#include <kernel/arch/cpu.h>

#include <boot/stage2.h>

#include <libc/string.h>

#define BOCHS_E9_HACK 0

static const int dbg_baud_rate = 115200;

int arch_dbg_con_init(kernel_args *ka)
{
	short divisor = 115200 / dbg_baud_rate;

	out8(0x80, 0x3fb);	/* set up to load divisor latch	*/
	out8(divisor & 0xf, 0x3f8);		/* LSB */
	out8(divisor >> 8, 0x3f9);		/* MSB */
	out8(3, 0x3fb);		/* 8N1 */

	return 0;
}

char arch_dbg_con_read()
{
	while ((in8(0x3fd) & 1) == 0)
		;
		
	return in8(0x3f8);
}

static void _arch_dbg_con_putch(const char c)
{
#if BOCHS_E9_HACK
	out8(c, 0xe9);
#else
	while ((in8(0x3fd) & 0x64) == 0)
		;
		
	out8(c, 0x3f8);
#endif
}

char arch_dbg_con_putch(const char c)
{
	if (c == '\n') {
		_arch_dbg_con_putch('\r');
		_arch_dbg_con_putch('\n');
	} else if (c != '\r')
		_arch_dbg_con_putch(c);

	return c;
}

void arch_dbg_con_puts(const char *s)
{
	while(*s != '\0') {
		arch_dbg_con_putch(*s);
		s++;
	}
}

