#include <kernel.h>
#include <console.h>
#include <debug.h>
#include <int.h>
#include <smp.h>
#include <sem.h>

//#include <arch_console.h>

#include <stdarg.h>
#include <printf.h>

//static sem_id console_sem = -1;

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
	char ret = c;

/*
	sem_acquire(console_sem, 1);

	ret = arch_con_putch(c);

	sem_release(console_sem, 1);
*/	
	return ret;
}

void con_puts(const char *s)
{
	TOUCH(s);
/*
	sem_acquire(console_sem, 1);

	arch_con_puts(s);

	sem_release(console_sem, 1);
*/
}

void con_puts_xy(const char *s, int x, int y)
{
	TOUCH(s);TOUCH(x);TOUCH(y);
/*
	sem_acquire(console_sem, 1);

	arch_con_puts_xy(s, x, y);

	sem_release(console_sem, 1);
*/
}

int con_init(kernel_args *ka)
{
	dprintf("con_init: entry\n");

	TOUCH(ka);
/*
	console_sem = sem_create(1, "console_sem");
	if(console_sem < 0)
		panic("error creating console semaphore\n");
	return arch_con_init(ka);
*/
	return 0;
}
