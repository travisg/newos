#include <kernel.h>
#include <stage2.h>

int arch_pmap_init(kernel_args *ka)
{
	return 0;
}

int arch_pmap_init2(kernel_args *ka)
{
	return 0;
}

int pmap_map_page(addr paddr, addr vaddr, int lock)
{
	return 0;
}

int pmap_unmap_page(addr vaddr)
{
	return 0;
}

void arch_pmap_invl_page(addr vaddr)
{
	return;
}

