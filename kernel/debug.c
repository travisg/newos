#include "kernel.h"
#include "debug.h"
#include "int.h"
#include "spinlock.h"

#include "arch_dbg_console.h"

#include <stdarg.h>
#include <printf.h>

static bool serial_debug_on = false;
static int dbg_spinlock = 0;

int dprintf(const char *fmt, ...)
{
	int ret = 0; 
	va_list args;
	char temp[128];

	if(serial_debug_on) {
		va_start(args, fmt);
		ret = vsprintf(temp, fmt, args);
		va_end(args);
	
		dbg_puts(temp);
	}
	return ret;
}

char dbg_putch(char c)
{
	char ret;
	int flags = int_disable_interrupts();
	acquire_spinlock(&dbg_spinlock);
	
	if(serial_debug_on)
		ret = arch_dbg_con_putch(c);
	else
		ret = c;

	release_spinlock(&dbg_spinlock);
	int_restore_interrupts(flags);

	return ret;
}

void dbg_puts(const char *s)
{
	int flags = int_disable_interrupts();
	acquire_spinlock(&dbg_spinlock);
	
	if(serial_debug_on)
		arch_dbg_con_puts(s);

	release_spinlock(&dbg_spinlock);
	int_restore_interrupts(flags);
}

int dbg_init(struct kernel_args *ka)
{
	return arch_dbg_con_init(ka);
}

bool dbg_set_serial_debug(bool new_val)
{
	int temp = serial_debug_on;
	serial_debug_on = new_val;
	return temp;
}

