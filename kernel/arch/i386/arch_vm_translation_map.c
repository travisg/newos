/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/arch/vm_translation_map.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/vm.h>
#include <kernel/vm_page.h>
#include <kernel/vm_priv.h>
#include <kernel/arch/cpu.h>
#include <kernel/debug.h>
#include <libc/string.h>
#include <boot/stage2.h>

typedef struct vm_translation_map_arch_info_struct {
	pdentry *pgdir_virt;
	pdentry *pgdir_phys;
} vm_translation_map_arch_info;

static ptentry *page_hole = NULL;
static pdentry *curr_pgdir = NULL;
static pdentry *kernel_pgdir_phys = NULL;
static pdentry *kernel_pgdir_virt = NULL;

static vm_translation_map *tmap_list;
static int tmap_list_lock;

#define CHATTY_TMAP 0

#define ADDR_SHIFT(x) ((x)>>12)

#define VADDR_TO_PDENT(va) ((va) / PAGE_SIZE / 1024)

#define FIRST_USER_PGDIR_ENT    (VADDR_TO_PDENT(USER_BASE))
#define NUM_USER_PGDIR_ENTS     (VADDR_TO_PDENT(USER_SIZE))
#define FIRST_KERNEL_PGDIR_ENT  (VADDR_TO_PDENT(KERNEL_BASE))
#define NUM_KERNEL_PGDIR_ENTS   (VADDR_TO_PDENT(KERNEL_SIZE))

static void invl_page(addr vaddr)
{
	invalidate_TLB(vaddr);
}

static void init_pdentry(pdentry *e)
{
	*(int *)e = 0;
}

static void init_ptentry(ptentry *e)
{
	*(int *)e = 0;
}

static void _update_all_pgdirs(int index, pdentry e)
{
	// XXX disable interrupts here?
	unsigned int state;
	vm_translation_map *entry;
	
	state = int_disable_interrupts();
	acquire_spinlock(&tmap_list_lock);
	
	for(entry = tmap_list; entry != NULL; entry = entry->next)
		entry->arch_data->pgdir_virt[index] = e;

	release_spinlock(&tmap_list_lock);
	int_restore_interrupts(state);
}

static void destroy_tmap(vm_translation_map *map)
{
	int state;
	vm_translation_map *entry;
	vm_translation_map *last = NULL;

	if(map == NULL)
		return;

	// remove it from the tmap list
	state = int_disable_interrupts();
	acquire_spinlock(&tmap_list_lock);

	entry = tmap_list;
	while(entry != NULL) {
		if(entry == map) {
			if(last != NULL) {
				last->next = entry->next;
			} else {
				tmap_list = entry->next;
			}
			break;
		}
		last = entry;
		entry = entry->next;
	}

	release_spinlock(&tmap_list_lock);
	int_restore_interrupts(state);

	if(map->arch_data->pgdir_virt != NULL)
		kfree(map->arch_data->pgdir_virt);

	kfree(map->arch_data);
}

static int lock_tmap(vm_translation_map *map)
{
	acquire_spinlock(&map->lock);	

	return -1;
}

static int unlock_tmap(vm_translation_map *map)
{
	release_spinlock(&map->lock);

	return -1;
}

// XXX currently assumes this translation map is active
static int map_tmap(vm_translation_map *map, addr va, addr pa, unsigned int attributes)
{
	ptentry *pentry;
	int index;

#if CHATTY_TMAP
	dprintf("map_tmap: entry pa 0x%x va 0x%x\n", pa, va);
#endif
/*
	dprintf("pgdir at 0x%x\n", pgdir);
	dprintf("index is %d\n", va / PAGE_SIZE / 1024);
	dprintf("final at 0x%x\n", &pgdir[va / PAGE_SIZE / 1024]);
	dprintf("value is 0x%x\n", *(int *)&pgdir[va / PAGE_SIZE / 1024]);
	dprintf("present bit is %d\n", pgdir[va / PAGE_SIZE / 1024].present);
	dprintf("addr is %d\n", pgdir[va / PAGE_SIZE / 1024].addr);
*/
	// check to see if a page table exists for this range
	index = VADDR_TO_PDENT(va);
	if(curr_pgdir[index].present == 0) {
		addr pgtable;
		pdentry *e;
		vm_page *page;
		// we need to allocate a pgtable
		page = vm_page_allocate_page(PAGE_STATE_FREE);
		pgtable = page->ppn * PAGE_SIZE;
#if CHATTY_TMAP
		dprintf("map_tmap: asked for free page for pgtable. 0x%x\n", pgtable);
#endif

		// put it in the pgdir
		e = &curr_pgdir[index];
		init_pdentry(e);
		e->addr = ADDR_SHIFT(pgtable);
		e->supervisor = !(attributes & LOCK_KERNEL);
		e->rw = attributes & LOCK_RW;
		e->present = 1;

		// zero it out in it's new mapping
		memset((unsigned int *)((unsigned int)page_hole + (va / PAGE_SIZE / 1024) * PAGE_SIZE), 0, PAGE_SIZE);
		
		// update any other page directories
		_update_all_pgdirs(index, *e);
		
		map->map_count++;
	}
	// now, fill in the pentry
	pentry = page_hole + va / PAGE_SIZE;

	init_ptentry(pentry);
	pentry->addr = ADDR_SHIFT(pa);
	pentry->supervisor = !(attributes & LOCK_KERNEL);
	pentry->rw = attributes & LOCK_RW;
	pentry->present = 1;

	invl_page(va);
	smp_send_broadcast_ici(SMP_MSG_INVL_PAGE, va, NULL, SMP_MSG_FLAG_ASYNC);
	
	map->map_count++;

	return 0;
}

// XXX currently assumes this translation map is active
static int unmap_tmap(vm_translation_map *map, addr start, addr end)
{
	ptentry *pentry;

	if((end - start) > PAGE_SIZE) {
		// XXX finish
		panic("unmap_tmap: asked to unmap more than one page! not currently supported\n");
	}

	if(curr_pgdir[VADDR_TO_PDENT(start)].present) {
		pentry = page_hole + start / PAGE_SIZE;
		pentry->present = 0;
		map->map_count--;
	}
	return 0;
}

// XXX currently assumes this translation map is active
static int query_tmap(vm_translation_map *map, addr va, addr *out_physical, unsigned int *out_flags)
{
	ptentry *pentry;

	if(curr_pgdir[VADDR_TO_PDENT(va)].present == 0) {
		// no pagetable here
		return -1;
	}

	pentry = page_hole + va / PAGE_SIZE;
	if(pentry->present == 0) {
		// page mapping not valid
		return -1;
	}
	
	*out_physical = pentry->addr << 12;
	
	// XXX fill this
	*out_flags = 0;

	return 0;	
}

static addr get_mapped_size_tmap(vm_translation_map *map)
{
	return map->map_count;
}

static int protect_tmap(vm_translation_map *map, addr base, addr top, unsigned int attributes)
{
	// XXX finish
	return -1;
}

static vm_translation_map_ops tmap_ops = {
	destroy_tmap,
	lock_tmap,
	unlock_tmap,
	map_tmap,
	unmap_tmap,
	query_tmap,
	get_mapped_size_tmap,
	protect_tmap
};

int vm_translation_map_create(vm_translation_map *new_map, bool kernel)
{
	if(new_map == NULL)
		return -1;
	
	// initialize the new object
	new_map->ops = &tmap_ops;
	new_map->lock = 0;
	new_map->map_count = 0;

	new_map->arch_data = kmalloc(sizeof(vm_translation_map_arch_info));
	if(new_map == NULL)
		panic("error allocating translation map object!\n");

	if(!kernel) {
		// user
		// allocate a pgdir
		new_map->arch_data->pgdir_virt = kmalloc(PAGE_SIZE);
		if(new_map->arch_data->pgdir_virt == NULL) {
			kfree(new_map->arch_data);
			return -1;
		}
		if(((addr)new_map->arch_data->pgdir_virt % PAGE_SIZE) != 0)
			panic("vm_translation_map_create: malloced pgdir and found it wasn't aligned!\n");
		vm_get_page_mapping(vm_get_kernel_aspace(), (addr)new_map->arch_data->pgdir_virt, (addr *)&new_map->arch_data->pgdir_phys);
	} else {
		// kernel
		// we already know the kernel pgdir mapping
		(addr)new_map->arch_data->pgdir_virt = kernel_pgdir_virt;
		(addr)new_map->arch_data->pgdir_phys = kernel_pgdir_phys;
	}

	// zero out the bottom portion of the new pgdir
	memset(new_map->arch_data->pgdir_virt + FIRST_USER_PGDIR_ENT, 0, NUM_USER_PGDIR_ENTS * sizeof(pdentry));
	
	// insert this new map into the map list
	{
		int state = int_disable_interrupts();
		acquire_spinlock(&tmap_list_lock);
		
		// copy the top portion of the pgdir from the current one
		memcpy(new_map->arch_data->pgdir_virt + FIRST_KERNEL_PGDIR_ENT, curr_pgdir + FIRST_KERNEL_PGDIR_ENT,
			NUM_KERNEL_PGDIR_ENTS * sizeof(pdentry));

		new_map->next = tmap_list;
		tmap_list = new_map;
	
		release_spinlock(&tmap_list_lock);
		int_restore_interrupts(state);
	}

	return 0;
}

int vm_translation_map_module_init(kernel_args *ka)
{
	// page hole set up in stage2
	page_hole = (ptentry *)ka->arch_args.page_hole;
	// calculate where the pgdir would be
	curr_pgdir = (pdentry *)(((unsigned int)ka->arch_args.page_hole) + (PAGE_SIZE * 1024 - PAGE_SIZE));
	// clear out the bottom 2 GB, unmap everything
	memset(curr_pgdir + FIRST_USER_PGDIR_ENT, 0, sizeof(pdentry) * NUM_USER_PGDIR_ENTS);

	kernel_pgdir_phys = (pdentry *)ka->arch_args.phys_pgdir;
	kernel_pgdir_virt = (pdentry *)ka->arch_args.vir_pgdir;

	tmap_list_lock = 0;
	tmap_list = NULL;

	return 0;
}

int vm_translation_map_module_init2(kernel_args *ka)
{
	// now that the vm is initialized, create an region that represents
	// the page hole
	void *temp;
	
	dprintf("vm_translation_map_module_init2: entry\n");

	temp = (void *)page_hole;
	vm_create_anonymous_region(vm_get_kernel_aspace(), "page_hole", &temp,
		REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE * 1024, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
	temp = (void *)ka->arch_args.vir_pgdir;
	vm_create_anonymous_region(vm_get_kernel_aspace(), "kernel_pgdir", &temp,
		REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	return 0;
}

// XXX horrible back door to map a page quickly regardless of translation map object, etc.
// used only during VM setup
int vm_translation_map_quick_map(kernel_args *ka, addr va, addr pa, unsigned int attributes, addr (*get_free_page)(kernel_args *))
{
	ptentry *pentry;
	int index;

#if CHATTY_TMAP
	dprintf("quick_tmap: entry pa 0x%x va 0x%x\n", pa, va);
#endif

	// check to see if a page table exists for this range
	index = VADDR_TO_PDENT(va);
	if(curr_pgdir[index].present == 0) {
		addr pgtable;
		pdentry *e;
		// we need to allocate a pgtable
		pgtable = get_free_page(ka);
		// pgtable is in pages, convert to physical address
		pgtable *= PAGE_SIZE;
#if CHATTY_TMAP
		dprintf("quick_map: asked for free page for pgtable. 0x%x\n", pgtable);
#endif

		// put it in the pgdir
		e = &curr_pgdir[index];
		init_pdentry(e);
		e->addr = ADDR_SHIFT(pgtable);
		e->supervisor = !(attributes & LOCK_KERNEL);
		e->rw = attributes & LOCK_RW;
		e->present = 1;

		// zero it out in it's new mapping
		memset((unsigned int *)((unsigned int)page_hole + (va / PAGE_SIZE / 1024) * PAGE_SIZE), 0, PAGE_SIZE);
	}
	// now, fill in the pentry
	pentry = page_hole + va / PAGE_SIZE;

	init_ptentry(pentry);
	pentry->addr = ADDR_SHIFT(pa);
	pentry->supervisor = !(attributes & LOCK_KERNEL);
	pentry->rw = attributes & LOCK_RW;
	pentry->present = 1;

	invl_page(va);

	return 0;
}

addr vm_translation_map_get_pgdir(vm_translation_map *map)
{
	return (addr)map->arch_data->pgdir_phys;
}
