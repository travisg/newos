#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/debug.h>

#include <kernel/arch/vm.h>
#include <kernel/arch/int.h>
#include <kernel/arch/pmap.h>
#include <kernel/arch/cpu.h>

#include <kernel/arch/i386/interrupts.h>

int arch_vm_init(kernel_args *ka)
{
	dprintf("arch_vm_init: entry\n");	
	return 0;
}

int arch_vm_init2(kernel_args *ka)
{
	unsigned int *pgdir;
	
	dprintf("arch_vm_init2: entry\n");

	// account for BIOS area and mark the pages unusable
	vm_mark_page_range_inuse(0xa0000 / PAGE_SIZE, (0x100000 - 0xa0000) / PAGE_SIZE);

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

