#include <stage2.h>

int arch_dbg_con_init(kernel_args *ka)
{
	return 0;
}

char arch_dbg_con_read()
{
	return 0;
}

static void _arch_dbg_con_putch(const char c)
{
	return;
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

