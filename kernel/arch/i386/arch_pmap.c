#include <kernel/kernel.h>

#include <kernel/vm.h>
#include <kernel/heap.h>
#include <kernel/vm_priv.h>
#include <kernel/debug.h>
#include <kernel/smp.h>
#include <kernel/int.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/pmap.h>
#include <kernel/arch/int.h>

#include <kernel/arch/i386/pmap.h>

#include <libc/string.h>

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
static pdentry *curr_pgdir = NULL;

// The following deal with a list of pgdirs that this module has to
// keep around so it can keep the kernel portion of the pgdirs
// in sync.
struct pgdir_list_entry {
	struct pgdir_list_entry *next;
	pdentry *pgdir;
};

static int pmap_pgdir_lock = 0;
static struct pgdir_list_entry *pgdir_list = NULL;

#define CHATTY_PMAP 0

#define ADDR_SHIFT(x) ((x)>>12)

#define VADDR_TO_PDENT(va) ((va) / PAGE_SIZE / 1024)

#define FIRST_USER_PGDIR_ENT    (VADDR_TO_PDENT(USER_BASE))
#define NUM_USER_PGDIR_ENTS     (VADDR_TO_PDENT(USER_SIZE))
#define FIRST_KERNEL_PGDIR_ENT  (VADDR_TO_PDENT(KERNEL_BASE))
#define NUM_KERNEL_PGDIR_ENTS   (VADDR_TO_PDENT(KERNEL_SIZE))

int arch_pmap_init(kernel_args *ka)
{
	dprintf("arch_pmap_init: entry\n");

	// page hole set up in stage2
	page_hole = (ptentry *)ka->arch_args.page_hole;
	// calculate where the pgdir would be
	curr_pgdir = (pdentry *)(((unsigned int)ka->arch_args.page_hole) + (PAGE_SIZE * 1024 - PAGE_SIZE));
	// clear out the bottom 2 GB, unmap everything
	memset(curr_pgdir + FIRST_USER_PGDIR_ENT, 0, sizeof(pdentry) * NUM_USER_PGDIR_ENTS);

	pmap_pgdir_lock = 0;
	pgdir_list = NULL;

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

static void pmap_update_all_pgdirs(int index, pdentry e)
{
	// XXX disable interrupts here?
	unsigned int state;
	struct pgdir_list_entry *entry;
	
	state = int_disable_interrupts();
	acquire_spinlock(&pmap_pgdir_lock);
	
	for(entry = pgdir_list; entry != NULL; entry = entry->next)
		entry->pgdir[index] = e;

	release_spinlock(&pmap_pgdir_lock);
	int_restore_interrupts(state);
}

int pmap_map_page(addr paddr, addr vaddr, int lock)
{
	ptentry *pentry;
	int index;

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
	index = VADDR_TO_PDENT(vaddr);
	if(curr_pgdir[index].present == 0) {
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
		e = &curr_pgdir[index];
		init_pdentry(e);
		e->addr = ADDR_SHIFT(pgtable);
		e->supervisor = !(lock & LOCK_KERNEL);
		e->rw = lock & LOCK_RW;
		e->present = 1;

		// zero it out in it's new mapping
		memset((unsigned int *)((unsigned int)page_hole + (vaddr / PAGE_SIZE / 1024) * PAGE_SIZE), 0, PAGE_SIZE);
		
		// update any other page directories
		pmap_update_all_pgdirs(index, *e);
	}
	// now, fill in the pentry
	pentry = page_hole + vaddr / PAGE_SIZE;

	init_ptentry(pentry);
	pentry->addr = ADDR_SHIFT(paddr);
	pentry->supervisor = !(lock & LOCK_KERNEL);
	pentry->rw = lock & LOCK_RW;
	pentry->present = 1;

	arch_pmap_invl_page(vaddr);
	smp_send_broadcast_ici(SMP_MSG_INVL_PAGE, vaddr, NULL, SMP_MSG_FLAG_ASYNC);
	
	return 0;
}

int pmap_unmap_page(addr vaddr)
{
	ptentry *pentry;

	if(curr_pgdir[VADDR_TO_PDENT(vaddr)].present) {
		pentry = page_hole + vaddr / PAGE_SIZE;
		pentry->present = 0;
	}
	return 0;
}

void arch_pmap_invl_page(addr vaddr)
{
	invalidate_TLB(vaddr);
}

int pmap_get_page_mapping(addr vaddr, addr *paddr)
{
	ptentry *pentry;

	if(curr_pgdir[VADDR_TO_PDENT(vaddr)].present == 0) {
		// no pagetable here
		return -1;
	}

	pentry = page_hole + vaddr / PAGE_SIZE;
	if(pentry->present == 0) {
		// page mapping not valid
		return -1;
	}
	
	*paddr = pentry->addr << 12;

	return 0;	
}

int pmap_init_and_add_pgdir_to_list(addr pgdir)
{
	unsigned int state;
	struct pgdir_list_entry *entry;
	
	entry = kmalloc(sizeof(struct pgdir_list_entry));
	if(entry == NULL) {
		panic("pmap_add_pgdir_to_list: unable to allocate pgdir container struct!\n");
		return -1;
	}
	entry->pgdir = (pdentry *)pgdir;
	
	// zero out the bottom portion of the new pgdir
	memset(entry->pgdir + FIRST_USER_PGDIR_ENT, 0, NUM_USER_PGDIR_ENTS * sizeof(pdentry));
	
	state = int_disable_interrupts();
	acquire_spinlock(&pmap_pgdir_lock);
	
	// copy the top portion of the pgdir from the current one
	memcpy(entry->pgdir + FIRST_KERNEL_PGDIR_ENT, curr_pgdir + FIRST_KERNEL_PGDIR_ENT,
		NUM_KERNEL_PGDIR_ENTS * sizeof(pdentry));
	
	entry->next = pgdir_list;
	pgdir_list = entry;

	release_spinlock(&pmap_pgdir_lock);
	int_restore_interrupts(state);

	return 0;
}

int pmap_remove_pgdir_from_list(addr pgdir)
{
	unsigned int state;
	struct pgdir_list_entry *last = NULL;
	struct pgdir_list_entry *entry;

	state = int_disable_interrupts();
	acquire_spinlock(&pmap_pgdir_lock);
	
	entry = pgdir_list;
	while(entry != NULL) {
		if(entry->pgdir == (pdentry *)pgdir) {
			if(last != NULL) {
				last->next = entry->next;
			} else {
				pgdir_list = entry->next;
			}
			break;
		}
		last = entry;
		entry = entry->next;
	}

	release_spinlock(&pmap_pgdir_lock);
	int_restore_interrupts(state);
	
	if(entry->pgdir == (pdentry *)pgdir)
		kfree(entry);
	return 0;
}

