#include <kernel.h>
#include <string.h>
#include <int.h>

#include <arch_cpu.h>

#include <stage2.h>

static const int dbg_baud_rate = 115200;

int arch_dbg_con_init(kernel_args *ka)
{
	short divisor = 115200 / dbg_baud_rate;
	TOUCH(ka);

	outb(0x80, 0x3fb);	/* set up to load divisor latch	*/
	outb(divisor & 0xf, 0x3f8);		/* LSB */
	outb(divisor >> 8, 0x3f9);		/* MSB */
	outb(3, 0x3fb);		/* 8N1 */

	return 0;
}

char arch_dbg_con_read()
{
	while ((inb(0x3fd) & 1) == 0)
		;
		
	return inb(0x3f8);
}

static void _arch_dbg_con_putch(const char c)
{
	while ((inb(0x3fd) & 0x64) == 0)
		;
		
	outb(c, 0x3f8);
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

