#include <string.h>
#include <kernel/kernel.h>

#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/debug.h>
#include <kernel/smp.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/pmap.h>
#include <kernel/arch/int.h>

typedef struct ptentry {
	unsigned int present:1;
	unsigned int rw:1;
	unsigned int supervisor:1;
	unsigned int write_through:1;
	unsigned int cache_disabled:1;
	unsigned int accessed:1;
	unsigned int dirty:1;
	unsigned int reserved:1;
	unsigned int global:1;
	unsigned int avail:3;
	unsigned int addr:20;
} ptentry;

typedef struct pdentry {
	unsigned int present:1;
	unsigned int rw:1;
	unsigned int supervisor:1;
	unsigned int write_through:1;
	unsigned int cache_disabled:1;
	unsigned int accessed:1;
	unsigned int reserved:1;
	unsigned int page_size:1;
	unsigned int global:1;
	unsigned int avail:3;
	unsigned int addr:20;
} pdentry;

static ptentry *page_hole = NULL;
static pdentry *pgdir = NULL;

#define CHATTY_PMAP 0

#define ADDR_SHIFT(x) ((x)>>12)

int arch_pmap_init(kernel_args *ka)
{
	dprintf("arch_pmap_init: entry\n");

	// page hole set up in stage2
	page_hole = (ptentry *)ka->arch_args.page_hole;
	// calculate where the pgdir would be
	pgdir = (pdentry *)(((unsigned int)ka->arch_args.page_hole) + (PAGE_SIZE * 1024 - PAGE_SIZE));
	// clear out the bottom 2 GB, unmap everything
	memset(pgdir, 0, sizeof(unsigned int) * 512);

	return 0;
}

int arch_pmap_init2(kernel_args *ka)
{
	// now that the vm is initialized, create an area that represents
	// the page hole
	void *temp;
	
	dprintf("arch_pmap_init2: entry\n");

	temp = (void *)page_hole;
	vm_create_area(vm_get_kernel_aspace(), "page_hole", &temp, AREA_ALREADY_MAPPED, PAGE_SIZE * 1024, LOCK_RW|LOCK_KERNEL);
	temp = (void *)ka->arch_args.vir_pgdir;
	vm_create_area(vm_get_kernel_aspace(), "kernel_pgdir", &temp, AREA_ALREADY_MAPPED, PAGE_SIZE, LOCK_RW|LOCK_KERNEL);

	return 0;
}

static void init_pdentry(pdentry *e)
{
	*(int *)e = 0;
}

static void init_ptentry(ptentry *e)
{
	*(int *)e = 0;
}

int pmap_map_page(addr paddr, addr vaddr, int lock)
{
	ptentry *pentry;

#if CHATTY_PMAP
	dprintf("pmap_map_page: entry paddr 0x%x vaddr 0x%x\n", paddr, vaddr);
#endif
/*
	dprintf("pgdir at 0x%x\n", pgdir);
	dprintf("index is %d\n", vaddr / PAGE_SIZE / 1024);
	dprintf("final at 0x%x\n", &pgdir[vaddr / PAGE_SIZE / 1024]);
	dprintf("value is 0x%x\n", *(int *)&pgdir[vaddr / PAGE_SIZE / 1024]);
	dprintf("present bit is %d\n", pgdir[vaddr / PAGE_SIZE / 1024].present);
	dprintf("addr is %d\n", pgdir[vaddr / PAGE_SIZE / 1024].addr);
*/
	// check to see if a page table exists for this range
	if(pgdir[vaddr / PAGE_SIZE / 1024].present == 0) {
		unsigned int pgtable;
		pdentry *e;
		// we need to allocate a pgtable
		vm_get_free_page(&pgtable);
		// pgtable is in pages, convert to physical address
		pgtable *= PAGE_SIZE;
#if CHATTY_PMAP
		dprintf("pmap_map_page: asked for free page for pgtable. 0x%x\n", pgtable);
#endif

		// put it in the pgdir
		e = &pgdir[vaddr / PAGE_SIZE / 1024];
		init_pdentry(e);
		e->addr = ADDR_SHIFT(pgtable);
		e->supervisor = lock & LOCK_KERNEL;
		e->rw = lock & LOCK_RW;
		e->present = 1;

		// zero it out in it's new mapping
		memset((unsigned int *)((unsigned int)page_hole + (vaddr / PAGE_SIZE / 1024) * PAGE_SIZE), 0, PAGE_SIZE);
	}
	// now, fill in the pentry
	pentry = page_hole + vaddr / PAGE_SIZE;

	init_ptentry(pentry);
	pentry->addr = ADDR_SHIFT(paddr);
	pentry->supervisor = lock & LOCK_KERNEL;
	pentry->rw = lock & LOCK_RW;
	pentry->present = 1;

	arch_pmap_invl_page(vaddr);
	smp_send_broadcast_ici(SMP_MSG_INVL_PAGE, vaddr, NULL, SMP_MSG_FLAG_ASYNC);
	
	return 0;
}

int pmap_unmap_page(addr vaddr)
{
	ptentry *pentry;

	if(pgdir[vaddr / PAGE_SIZE / 1024].present) {
		pentry = page_hole + vaddr / PAGE_SIZE;
		pentry->present = 0;
	}
	return 0;
}

void arch_pmap_invl_page(addr vaddr)
{
	invalidate_TLB(vaddr);
}

