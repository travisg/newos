#include <kernel.h>
#include <vm.h>
#include <debug.h>
#include <string.h>

#include <arch_pmap.h>

#include <stage2.h>
#include <sh4.h>
#include <vcpu.h>

#define CHATTY_PMAP 0

vcpu_struct *vcpu;

#define PHYS_TO_P1(x) ((x) + P1_AREA)

int arch_pmap_init(kernel_args *ka)
{
	dprintf("arch_pmap_init: entry\n");

	vcpu = ka->vcpu;
	
	dprintf("examining vcpu structure @ 0x%x:\n", vcpu);
	dprintf("kernel_pgdir = 0x%x\n", vcpu->kernel_pgdir);
	dprintf("user_pgdir = 0x%x\n", vcpu->user_pgdir);
	dprintf("kernel_asid = 0x%x\n", vcpu->kernel_asid);
	dprintf("user_asid = 0x%x\n", vcpu->user_asid);

	return 0;
}

int arch_pmap_init2(kernel_args *ka)
{
	return 0;
}

int pmap_map_page(addr paddr, addr vaddr, int lock)
{
	struct pdent *pd = NULL;
	struct ptent *pt;
	unsigned int index = 0;

#if CHATTY_PMAP
	dprintf("pmap_map_page: entry paddr 0x%x vaddr 0x%x lock 0x%x\n", paddr, vaddr, lock);
#endif

	if(vaddr < P1_AREA) {
		// XXX deal with this
		panic("pmap_map_page: cannot map user space!\n");
	} else if(vaddr >= P3_AREA && vaddr < P4_AREA) {
		pd = (struct pdent *)vcpu->kernel_pgdir;
		index = (vaddr & 0x7fffffff) >> 22;
	} else {
		// invalid area to map pages
		panic("pmap_map_page: invalid vaddr 0x%x\n", vaddr);
	}

	if(pd[index].v == 0) {
		// need to allocate a pagetable
		unsigned int pgtable;

		// get a free page for a pagetable
		vm_get_free_page(&pgtable);
		pgtable *= PAGE_SIZE;

		// zero out the page
		memset((void *)PHYS_TO_P1(pgtable), 0, PAGE_SIZE);

		// stick it in the page dir
		pd[index].ppn = pgtable >> 12;
		pd[index].v = 1;
	}

	// get the pagetable
	pt = (struct ptent *)PHYS_TO_P1(pd[index].ppn << 12);
	index = (vaddr >> 12) & 0x000003ff;
	
	// insert the mapping
	pt[index].wt = 0;
	pt[index].pr = (lock & LOCK_KERNEL) ? (lock & 0x1) : (lock | 0x2);
	pt[index].ppn = paddr >> 12;
	pt[index].tlb_ent = 0;
	pt[index].c = 1;
	pt[index].sz = 0x1; // 4k page
	pt[index].sh = 0; // XXX shared?
	pt[index].d = 0;
	pt[index].v = 1;

	arch_pmap_invl_page(vaddr);
	
	return 0;
}

int pmap_unmap_page(addr vaddr)
{
	panic("pmap_unmap_page unimplemented!\n");
	return 0;
}

void arch_pmap_invl_page(addr vaddr)
{
#if CHATTY_PMAP
	dprintf("arch_pmap_invl_page: vaddr 0x%x\n", vaddr);
#endif
	return;
}

