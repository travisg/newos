#include "arch_cpu.h"
#include "stage2.h"

int arch_pmap_init(struct kernel_args *ka)
{
	return 0;
}

int arch_pmap_init2(struct kernel_args *ka)
{
	return 0;
}

int pmap_map_page(unsigned int paddr, unsigned int vaddr)
{
	return 0;
}

int pmap_unmap_page(unsigned int vaddr)
{
	return 0;
}

