/*
** Copyright 2008, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include "stage2_priv.h"

// working pagedir and pagetables
static unsigned long *pgtable0 = 0;

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

static unsigned long *alloc_pagetable(kernel_args *ka)
{
    int i;
    unsigned long *table;

    table = (unsigned long *)next_paddr;
    next_paddr += PAGE_SIZE;
    ka->arch_args.pgtables[ka->arch_args.num_pgtables++] = (addr_t)table;

    // clear it out
    for (i = 0; i < 512; i++)
        table[i] = 0;

//  dprintf("alloc_pagetable returning %p\n", table);

    return table;
}

// allocate enough page tables to allow mapping of kernel and identity mapping of user space
//
// kernel maps at 0xffffffff00000000 (top of 64bit - 4GB)
int mmu_init(kernel_args *ka)
{
    ka->arch_args.num_pgtables = 0;

    // allocate a new top level pgdir
    pgtable0 = alloc_pagetable(ka);
    ka->arch_args.phys_pgdir = (addr_t)pgtable0;

    // set up the identity map of low ram
    setup_identity_map();

    dprintf("switching page dirs: new one at %p\n", pgtable0);

    // switch to the new pgdir
    asm("mov %0, %%rax;"
        "mov %%rax, %%cr3;" :: "m" (pgtable0) : "rax");

    dprintf("done switching page dirs\n");

    return 0;
}

#define PGTABLE0_ENTRY(vaddr) (((vaddr) >> 39) & 0x1ff)
#define PGTABLE1_ENTRY(vaddr) (((vaddr) >> 30) & 0x1ff)
#define PGTABLE2_ENTRY(vaddr) (((vaddr) >> 21) & 0x1ff)
#define PGTABLE3_ENTRY(vaddr) (((vaddr) >> 12) & 0x1ff)

#define PGENT_TO_ADDR(ent) ((ent) & 0x7ffffffffffff000UL)
#define PGENT_PRESENT(ent) ((ent) & 0x1)

static unsigned long *lookup_pgtable_entry(addr_t vaddr)
{
    vaddr &= ~(PAGE_SIZE-1);

//  dprintf("lookup_pgtable_entry: vaddr = (%d, %d, %d, %d)\n",
//      PGTABLE0_ENTRY(vaddr),
//      PGTABLE1_ENTRY(vaddr),
//      PGTABLE2_ENTRY(vaddr),
//      PGTABLE3_ENTRY(vaddr));

    // dive into the kernel page tables, allocating as they come up
    unsigned long *pgtable = pgtable0;
    unsigned long *ent = &pgtable[PGTABLE0_ENTRY(vaddr)];

    if (!PGENT_PRESENT(*ent)) {
        pgtable = alloc_pagetable(ka);
        *ent = (addr_t)pgtable | DEFAULT_PAGE_FLAGS;
    } else {
//      dprintf("existing ent 0x%lx\n", *ent);
        pgtable = (unsigned long *)PGENT_TO_ADDR(*ent);
    }
//  dprintf("pgtable_addr 0 %p\n", pgtable);

    ent = &pgtable[PGTABLE1_ENTRY(vaddr)];
    if (!PGENT_PRESENT(*ent)) {
        pgtable = alloc_pagetable(ka);
        *ent = (addr_t)pgtable | DEFAULT_PAGE_FLAGS;
    } else {
//      dprintf("existing ent 0x%lx\n", *ent);
        pgtable = (unsigned long *)PGENT_TO_ADDR(*ent);
    }
//  dprintf("pgtable_addr 1 %p\n", pgtable);

    ent = &pgtable[PGTABLE2_ENTRY(vaddr)];
    if (!PGENT_PRESENT(*ent)) {
        pgtable = alloc_pagetable(ka);
        *ent = (addr_t)pgtable | DEFAULT_PAGE_FLAGS;
    } else {
//      dprintf("existing ent 0x%lx\n", *ent);
        pgtable = (unsigned long *)PGENT_TO_ADDR(*ent);
    }
//  dprintf("pgtable_addr 2 %p\n", pgtable);

    // now map it

    return &pgtable[PGTABLE3_ENTRY(vaddr)];
}

// can only map in kernel space
void mmu_map_page(addr_t vaddr, addr_t paddr)
{
//  dprintf("mmu_map_page: vaddr 0x%lx, paddr 0x%lx\n", vaddr, paddr);

    unsigned long *pgtable_entry;

    pgtable_entry = lookup_pgtable_entry(vaddr);

    paddr &= ~(PAGE_SIZE-1);
    *pgtable_entry = paddr | DEFAULT_PAGE_FLAGS;
}

