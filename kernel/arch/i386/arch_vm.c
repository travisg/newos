#include "console.h"
#include "vm.h"
#include "debug.h"

#include "arch_vm.h"
#include "arch_interrupts.h"
#include "arch_int.h"
#include "arch_pmap.h"
#include "arch_cpu.h"

unsigned int *gdt = 0;

int arch_vm_init(struct kernel_args *ka)
{
	dprintf("arch_vm_init: entry\n");	
	return 0;
}

int arch_vm_init2(struct kernel_args *ka)
{
	dprintf("arch_vm_init2: entry\n");
	
	set_intr_gate(14, &_page_fault_int);
	
	// account for BIOS area and mark the pages unusable
	vm_mark_page_range_inuse(0xa0000 / PAGE_SIZE, (0x100000 - 0xa0000) / PAGE_SIZE);

	// account for the segment descriptor
	gdt = (unsigned int *)ka->vir_gdt;	
	vm_map_physical_memory(vm_get_kernel_aspace(), "gdt", (void *)&gdt, AREA_ANY_ADDRESS, PAGE_SIZE, 0, ka->phys_gdt);
	
	return 0;
}

int map_page_into_kspace(unsigned int paddr, unsigned int kaddr)
{
	return pmap_map_page(paddr, kaddr);
}

void i386_page_fault(int cr2reg, int errorcode)
{
	vm_page_fault(cr2reg, errorcode);
}

