#include <kernel/arch/cpu.h>
#include <stage2.h>

char arch_con_putch(char c)
{
	return c;
}

void arch_con_puts(const char *s)
{
}

int arch_con_init(struct kernel_args *ka)
{
	return 0;
}

