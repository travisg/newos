#include <kernel/kernel.h>
#include <stage2.h>

int arch_vm_init(kernel_args *ka)
{
	return 0;
}

int arch_vm_init2(kernel_args *ka)
{
	return 0;
}

int map_page_into_kspace(addr paddr, addr kaddr, int lock)
{
	return 0;
}

