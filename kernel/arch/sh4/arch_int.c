#include <stage2.h>

void arch_int_enable_io_interrupt(int irq)
{
	return;
}

void arch_int_disable_io_interrupt(int irq)
{
	return;
}

void arch_int_enable_interrupts()
{
	return;
}

int arch_int_disable_interrupts()
{
	return 0;
}

void arch_int_restore_interrupts(int oldstate)
{
	return;
}

int arch_int_init(kernel_args *ka)
{
	return 0;
}

int arch_int_init2(kernel_args *ka)
{
	return 0;
}

