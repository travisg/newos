#include <arch_cpu.h>
#include <stage2.h>
#include <vm.h>
#include <arch_pmap.h>

int arch_vm_init(struct kernel_args *ka)
{
	return 0;
}

int arch_vm_init2(struct kernel_args *ka)
{
	return 0;
}

int map_page_into_kspace(unsigned int paddr, unsigned int kaddr)
{
	return pmap_map_page(paddr, kaddr);
}

void sh4_page_fault(int cr2reg, int errorcode)
{
	vm_page_fault(cr2reg, errorcode);
}

