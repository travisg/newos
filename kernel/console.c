#include "kernel.h"
#include "console.h"
#include "debug.h"
#include "int.h"

#include "arch_console.h"

#include <stdarg.h>
#include <printf.h>

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

	ret = arch_con_putch(c);

	int_restore_interrupts(flags);

	return ret;
}

void con_puts(const char *s)
{
	int flags = int_disable_interrupts();

	arch_con_puts(s);

	int_restore_interrupts(flags);
}

void con_puts_xy(const char *s, int x, int y)
{
	int flags = int_disable_interrupts();

	arch_con_puts_xy(s, x, y);

	int_restore_interrupts(flags);
}

int con_init(struct kernel_args *ka)
{
	dprintf("con_init: entry\n");
	return arch_con_init(ka);
}

