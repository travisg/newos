#include "string.h"
#include "kernel.h"

#include "vm.h"
#include "debug.h"
#include "smp.h"

#include "arch_cpu.h"
#include "arch_pmap.h"
#include "arch_int.h"

#define DEFAULT_PAGE_FLAGS (1 | 2 | 256) // present/rw/global
#define ADDR_MASK 0xfffff000

static unsigned int *page_hole = NULL;
static unsigned int *pgdir = NULL;

#define CHATTY_PMAP 0

int arch_pmap_init(struct kernel_args *ka)
{
	dprintf("arch_pmap_init: entry\n");

	// page hole set up in stage2
	page_hole = (unsigned int *)ka->page_hole;
	// calculate where the pgdir would be
	pgdir = (unsigned int *)(((unsigned int)ka->page_hole) + (PAGE_SIZE * 1024 - PAGE_SIZE));
	// clear out the bottom 2 GB, unmap everything
	memset(pgdir, 0, sizeof(unsigned int) * 512);

	return 0;
}

int arch_pmap_init2(struct kernel_args *ka)
{
	// now that the vm is initialized, create an area that represents
	// the page hole
	void *temp = (void *)page_hole;
	TOUCH(ka);
	
	dprintf("arch_pmap_init2: entry\n");

	vm_create_area(vm_get_kernel_aspace(), "page_hole", &temp, AREA_ALREADY_MAPPED, PAGE_SIZE * 1024, 0);

	return 0;
}

int pmap_map_page(unsigned int paddr, unsigned int vaddr)
{
	unsigned int *pentry;

#if CHATTY_PMAP
	dprintf("pmap_map_page: entry paddr 0x%x vaddr 0x%x\n", paddr, vaddr);
#endif
	// check to see if a page table exists for this range
	if(pgdir[vaddr / PAGE_SIZE / 1024] == 0) {
		unsigned int pgtable;
		// we need to allocate a pgtable
		vm_get_free_page(&pgtable);
		// pgtable is in pages, convert to physical address
		pgtable *= PAGE_SIZE;
#if CHATTY_PMAP
		dprintf("pmap_map_page: asked for free page for pgtable. 0x%x\n", pgtable);
#endif

		// put it in the pgdir
		pgdir[vaddr / PAGE_SIZE / 1024] = (pgtable & ADDR_MASK) | DEFAULT_PAGE_FLAGS;

		// zero it out in it's new mapping
		memset((unsigned int *)((unsigned int)page_hole + (vaddr / PAGE_SIZE / 1024) * PAGE_SIZE), 0, PAGE_SIZE);
//		memset((unsigned int *)pgtable, 0, PAGE_SIZE);
	}
	// now, fill in the pentry
	pentry = page_hole + vaddr / PAGE_SIZE;

	*pentry = (paddr & ADDR_MASK) | DEFAULT_PAGE_FLAGS;

	arch_pmap_invl_page(vaddr);
	smp_send_broadcast_ici(SMP_MSG_INVL_PAGE, vaddr, NULL);
	
	return 0;
}

int pmap_unmap_page(unsigned int vaddr)
{
	unsigned int *pentry;

	// XXX check for more validity other than '!= 0'
	if(pgdir[vaddr / PAGE_SIZE / 1024] != 0) {
		pentry = page_hole + vaddr / PAGE_SIZE;
		*pentry = 0;
	}
	return 0;
}

void arch_pmap_invl_page(unsigned int vaddr)
{
	invalidate_TLB(vaddr);
}

