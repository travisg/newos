#include "kernel.h"
#include "console.h"
#include "debug.h"
#include "int.h"
#include "spinlock.h"

#include "arch_console.h"

#include <stdarg.h>
#include <printf.h>

static int console_spinlock = 0;

int kprintf(const char *fmt, ...)
{
	int ret;
	va_list args;
	char temp[256];

	va_start(args, fmt);
	ret = vsprintf(temp,fmt,args);
	va_end(args);

	con_puts(temp);
	return ret;
}

int kprintf_xy(int x, int y, const char *fmt, ...)
{
	int ret;
	va_list args;
	char temp[256];

	va_start(args, fmt);
	ret = vsprintf(temp,fmt,args);
	va_end(args);

	con_puts_xy(temp, x, y);
	return ret;
}

char con_putch(char c)
{
	char ret;
	int flags = int_disable_interrupts();
	acquire_spinlock(&console_spinlock);

	ret = arch_con_putch(c);

	release_spinlock(&console_spinlock);
	int_restore_interrupts(flags);

	return ret;
}

void con_puts(const char *s)
{
	int flags = int_disable_interrupts();
	acquire_spinlock(&console_spinlock);

	arch_con_puts(s);

	release_spinlock(&console_spinlock);
	int_restore_interrupts(flags);
}

void con_puts_xy(const char *s, int x, int y)
{
	int flags = int_disable_interrupts();
	acquire_spinlock(&console_spinlock);

	arch_con_puts_xy(s, x, y);

	release_spinlock(&console_spinlock);
	int_restore_interrupts(flags);
}

int con_init(kernel_args *ka)
{
	dprintf("con_init: entry\n");

	console_spinlock = 0;

	return arch_con_init(ka);
}

