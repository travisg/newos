#include <string.h>
#include <printf.h>
#include "kernel.h"
#include "int.h"
#include "debug.h"
#include "stage2.h"
#include "arch_int.h"

int int_init(struct kernel_args *ka)
{
	dprintf("init_int_handlers: entry\n");
	return arch_int_init(ka);
}

int int_init2(struct kernel_args *ka)
{
	return arch_int_init2(ka);
}

#if !_INT_ARCH_INLINE_CODE
void int_enable_interrupts()
{
	arch_int_enable_interrupts();
}

int int_disable_interrupts()
{
	return arch_int_disable_interrupts();
}

void int_restore_interrupts(int oldstate)
{
	arch_int_restore_interrupts(oldstate);
}
#endif

