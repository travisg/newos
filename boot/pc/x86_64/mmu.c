/*
** Copyright 2008, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include "stage2_priv.h"

// working pagedir and pagetables
static unsigned long *pgtable0 = 0;
static unsigned long *pgtable1 = 0;
static unsigned long *pgtable2 = 0;

static unsigned long *pgtables_user = 0;

static void setup_identity_map(void)
{
	// build a set of page tables to keep the low identity map
	// the kernel will blow this away shortly
	pgtables_user = (unsigned long *)0x11000; // use this random spot

	// clear out all of these page tables
	memset(pgtables_user, 0, PAGE_SIZE * 2);

	// point the top level page directory at it
	pgtable0[0] = (addr_t)pgtables_user | DEFAULT_PAGE_FLAGS;

	// second level table points to the next
	pgtables_user[0] = ((addr_t)pgtables_user + PAGE_SIZE) | DEFAULT_PAGE_FLAGS;

	// third level page table identity maps 1GB using 2MB pages
	int i;
	for (i=0; i < 512; i++)
		pgtables_user[512 + i] = (i * 0x200000) | (1<<7) | DEFAULT_PAGE_FLAGS;
}

static unsigned long *alloc_pagetable(kernel_args *ka, addr_t *next_paddr)
{
	int i;
	unsigned long *table;

	table = (unsigned long *)*next_paddr;
	(*next_paddr) += PAGE_SIZE;
	ka->arch_args.pgtables[ka->arch_args.num_pgtables++] = (addr_t)table;

	// clear it out
	for(i = 0; i < 512; i++)
		table[i] = 0;

	return table;
}

// allocate enough page tables to allow mapping of kernel and identity mapping of user space
//
// kernel maps at 0xffffffff00000000 (top of 64bit - 4GB)
int mmu_init(kernel_args *ka, addr_t *next_paddr)
{
	ka->arch_args.num_pgtables = 0;

	// allocate a new top level pgdir
	pgtable0 = alloc_pagetable(ka, next_paddr);

	// set up the identity map of low ram
	setup_identity_map();

	// set up the kernel's 2nd level page table
	pgtable1 = alloc_pagetable(ka, next_paddr);

	// point the top level page table at it
	pgtable0[511] = (addr_t)pgtable1 | DEFAULT_PAGE_FLAGS;

	// create a 3rd level kernel pagetable
	pgtable2 = alloc_pagetable(ka, next_paddr);

	// point the 3nd level at 0xffffff00000000
	pgtable1[512 - 4] = (addr_t)pgtable2 | DEFAULT_PAGE_FLAGS;

	dprintf("switching page dirs: new one at %p\n", pgtable0);

	// switch to the new pgdir
	asm("mov %0, %%rax;"
		"mov %%rax, %%cr3;" :: "m" (pgtable0) : "rax");

	dprintf("done switching page dirs\n");

	return 0;
}

// can only map the 4 meg region right after KERNEL_BASE, may fix this later
// if need arises.
void mmu_map_page(addr_t vaddr, addr_t paddr)
{
	dprintf("mmu_map_page: vaddr 0x%lx, paddr 0x%lx\n", vaddr, paddr);

#if 0
	if(vaddr < KERNEL_BASE || vaddr >= (KERNEL_BASE + 4096*1024)) {
		dprintf("mmu_map_page: asked to map invalid page!\n");
		for(;;);
	}
	paddr &= ~(PAGE_SIZE-1);
//	dprintf("paddr 0x%x @ index %d\n", paddr, (vaddr % (PAGE_SIZE * 1024)) / PAGE_SIZE);
	pgtable[(vaddr % (PAGE_SIZE * 1024)) / PAGE_SIZE] = paddr | DEFAULT_PAGE_FLAGS;
#endif
}

