#include "arch_cpu.h"
#include "stage2.h"

int arch_dbg_con_init(struct kernel_args *ka)
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

