#include <kernel.h>
#include <console.h>
#include <vm.h>
#include <debug.h>

#include <arch_vm.h>
#include <arch_interrupts.h>
#include <arch_int.h>
#include <arch_pmap.h>
#include <arch_cpu.h>

unsigned int *gdt = 0;

int arch_vm_init(kernel_args *ka)
{
	dprintf("arch_vm_init: entry\n");	
	return 0;
}

int arch_vm_init2(kernel_args *ka)
{
	dprintf("arch_vm_init2: entry\n");
	
	// account for BIOS area and mark the pages unusable
	vm_mark_page_range_inuse(0xa0000 / PAGE_SIZE, (0x100000 - 0xa0000) / PAGE_SIZE);

	// account for the segment descriptor
	gdt = (unsigned int *)ka->vir_gdt;	
	vm_create_area(vm_get_kernel_aspace(), "gdt", (void *)&gdt, AREA_ALREADY_MAPPED, PAGE_SIZE, LOCK_RW|LOCK_KERNEL);
	
	return 0;
}

int map_page_into_kspace(addr paddr, addr kaddr, int lock)
{
	return pmap_map_page(paddr, kaddr, lock);
}

int i386_page_fault(int cr2reg, unsigned int fault_address)
{
	return vm_page_fault(cr2reg, fault_address);
}

