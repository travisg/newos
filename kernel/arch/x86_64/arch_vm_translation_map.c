/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
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
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/queue.h>
#include <string.h>
#include <boot/stage2.h>
#include <newos/errors.h>

static addr_t iospace_addr = 0xfffffffe00000000UL;
static addr_t iospace_len = 0x100000000UL;

static void *phys_to_virt(addr_t phys);

#define PGTABLE0_ENTRY(vaddr) (((vaddr) >> 39) & 0x1ff)
#define PGTABLE1_ENTRY(vaddr) (((vaddr) >> 30) & 0x1ff)
#define PGTABLE2_ENTRY(vaddr) (((vaddr) >> 21) & 0x1ff)
#define PGTABLE3_ENTRY(vaddr) (((vaddr) >> 12) & 0x1ff)

#define PGENT_TO_ADDR(ent) ((ent) & 0x7ffffffffffff000UL)
#define PGENT_PRESENT(ent) ((ent) & 0x1)

// vm_translation object stuff
typedef struct vm_translation_map_arch_info_struct {
	unsigned long *pgdir_virt;
	addr_t pgdir_phys;
	struct list_node pagetable_list; // list of physical pages allocated for page tables
} vm_translation_map_arch_info;

static addr_t kernel_pgdir_phys;
static addr_t kernel_pgdir_virt;

static int lock_tmap(vm_translation_map *map)
{
	dprintf("lock_tmap: map %p\n", map);

	if(recursive_lock_lock(&map->lock) == true) {
		// we were the first one to grab the lock
	}

	return 0;
}

static int unlock_tmap(vm_translation_map *map)
{
	dprintf("unlock_tmap: map %p\n", map);

	if(recursive_lock_get_recursion(&map->lock) == 1) {
		// we're about to release it for the last time
	}
	recursive_lock_unlock(&map->lock);
	return 0;
}

static void destroy_tmap(vm_translation_map *map)
{
	PANIC_UNIMPLEMENTED();
}

static int map_tmap(vm_translation_map *map, addr_t va, addr_t pa, unsigned int attributes)
{
	PANIC_UNIMPLEMENTED();
}

static int unmap_tmap(vm_translation_map *map, addr_t start, addr_t end)
{
	PANIC_UNIMPLEMENTED();
}

static int query_tmap(vm_translation_map *map, addr_t va, addr_t *out_physical, unsigned int *out_flags)
{
	int index;
	unsigned long *pgtable;

	dprintf("query_tmap: va 0x%lx\n", va);
	
	*out_flags = 0;
	*out_physical = 0;

	// level 0 pagetable
	pgtable = map->arch_data->pgdir_virt;
	index = PGTABLE0_ENTRY(va);
	if (!PGENT_PRESENT(pgtable[index]))
		return NO_ERROR;

	pgtable = (unsigned long *)phys_to_virt(PGENT_TO_ADDR(pgtable[index]));
	index = PGTABLE1_ENTRY(va);
	if (!PGENT_PRESENT(pgtable[index]))
		return NO_ERROR;
	
	pgtable = (unsigned long *)phys_to_virt(PGENT_TO_ADDR(pgtable[index]));
	index = PGTABLE2_ENTRY(va);
	if (!PGENT_PRESENT(pgtable[index]))
		return NO_ERROR;

	pgtable = (unsigned long *)phys_to_virt(PGENT_TO_ADDR(pgtable[index]));
	index = PGTABLE3_ENTRY(va);
	if (!PGENT_PRESENT(pgtable[index]))
		return NO_ERROR;

	*out_physical = PGENT_TO_ADDR(pgtable[index]);
	*out_flags = 0;
	*out_flags |= pgtable[index] & (1<<0) ? PAGE_PRESENT : 0;
	*out_flags |= pgtable[index] & (1<<1) ? LOCK_RW : LOCK_RO;
	*out_flags |= pgtable[index] & (1<<2) ? 0 : LOCK_KERNEL;
	*out_flags |= pgtable[index] & (1<<5) ? PAGE_ACCESSED : 0;
	*out_flags |= pgtable[index] & (1<<6) ? PAGE_MODIFIED : 0;
	
	return NO_ERROR;
}

static addr_t get_mapped_size_tmap(vm_translation_map *map)
{
	PANIC_UNIMPLEMENTED();
}

static int protect_tmap(vm_translation_map *map, addr_t base, addr_t top, unsigned int attributes)
{
	PANIC_UNIMPLEMENTED();
}

static int clear_flags_tmap(vm_translation_map *map, addr_t va, unsigned int flags)
{
	PANIC_UNIMPLEMENTED();
}

static void flush_tmap(vm_translation_map *map)
{
	PANIC_UNIMPLEMENTED();
}

static void *phys_to_virt(addr_t phys)
{
	return (void *)(iospace_addr + phys);
}

static int get_physical_page_tmap(addr_t pa, addr_t *va, int flags)
{
	// look up the page in iospace
	if (pa > iospace_len) {
		panic("pa 0x%lx is too large\n", pa);
	}

	*va = (addr_t)phys_to_virt(pa);

	return 0;
}

static int put_physical_page_tmap(addr_t va)
{
	ASSERT (va >= iospace_addr && va < (iospace_addr + iospace_len));

	return 0;
}

static vm_translation_map_ops tmap_ops = {
	destroy_tmap,
	lock_tmap,
	unlock_tmap,
	map_tmap,
	unmap_tmap,
	query_tmap,
	get_mapped_size_tmap,
	protect_tmap,
	clear_flags_tmap,
	flush_tmap,
	get_physical_page_tmap,
	put_physical_page_tmap
};

int vm_translation_map_create(vm_translation_map *new_map, bool kernel)
{
	ASSERT(new_map);

	// initialize the new object
	new_map->ops = &tmap_ops;
	new_map->map_count = 0;
	if(recursive_lock_create(&new_map->lock) < 0)
		return ERR_NO_MEMORY;

	new_map->arch_data = kmalloc(sizeof(vm_translation_map_arch_info));
	if(new_map->arch_data == NULL) {
		recursive_lock_destroy(&new_map->lock);
		return ERR_NO_MEMORY;
	}

	if (!kernel) {
		// user
		vm_page *page = vm_page_allocate_page(PAGE_STATE_CLEAR);
		list_add_head(&new_map->arch_data->pagetable_list, &page->queue_node);

		new_map->arch_data->pgdir_phys = page->ppn * PAGE_SIZE;
		get_physical_page_tmap(page->ppn * PAGE_SIZE, (addr_t *)&new_map->arch_data->pgdir_virt, PHYSICAL_PAGE_NO_WAIT);

		// copy the kernel bits into this one (one entry at the top)
		memcpy(new_map->arch_data->pgdir_virt + 256, (unsigned long *)kernel_pgdir_virt + 256, sizeof(unsigned long) * 256);
	} else {
		// kernel top level page dir is already allocated
		new_map->arch_data->pgdir_phys = kernel_pgdir_phys;
		new_map->arch_data->pgdir_virt = (unsigned long *)kernel_pgdir_virt;

		vm_page *page = vm_lookup_page(kernel_pgdir_phys / PAGE_SIZE);
		dprintf("page %p, state %d\n", page, page->state);
		list_add_head(&new_map->arch_data->pagetable_list, &page->queue_node);

		// zero out the bottom of it, where user space mappings would go
		memset(new_map->arch_data->pgdir_virt, 0, sizeof(unsigned long) * 256);

		// XXX account for prexisting kernel page tables
	}

	return 0;
}

// set up physical mapping
// XXX for now map the first 4GB to the chunk of address space just underneath
// the top 4GB of kernel space (0xfffffffe00000000)
// also relies on an identity mapping left over from the bootloader to still be in effect
static void setup_iospace(kernel_args *ka)
{
//	dprintf("%s: entry\n", __PRETTY_FUNCTION__);

	// find the root page table
	int index;
	unsigned long *rootpgtable;
	unsigned long *pgtable;

	// look up and dereference the first entry
	rootpgtable = (unsigned long *)ka->arch_args.phys_pgdir;
	index = PGTABLE0_ENTRY(iospace_addr);
	ASSERT(PGENT_PRESENT(rootpgtable[index]));
	rootpgtable = (unsigned long *)PGENT_TO_ADDR(rootpgtable[index]);

	// walk through the page tables, mapping 2MB pages
	addr_t offset = 0;
	while (offset < iospace_len) {
		// allocate a 3rd level page table
		addr_t ppage = vm_alloc_ppage_from_kernel_struct(ka);
		pgtable = (unsigned long *)(ppage << 12);

		// map it
		index = PGTABLE1_ENTRY(iospace_addr + offset);
		rootpgtable[index] = (ppage << 12) | 3;

//		dprintf("pagetable at 0x%lx, index %d, vaddr 0x%lx\n", (ppage << 12), index, iospace_addr + offset);

		// map a series of 2MB pages
		addr_t i;
		for (i = 0; i < 512; i++) {
			pgtable[i] = (offset + 0x200000 * i) | (1<<7) | 3;
//			dprintf("ent 0x%lx (%p)\n", pgtable[i], &pgtable[i]);
		}

		// move forward 1GB
		offset += 0x40000000;
	}
}

int vm_translation_map_module_init(kernel_args *ka)
{
	dprintf("vm_translation_map_module_init: entry\n");

	// set up physical memory mapping
	setup_iospace(ka);

	kernel_pgdir_phys = ka->arch_args.phys_pgdir;
	get_physical_page_tmap(kernel_pgdir_phys, &kernel_pgdir_virt, PHYSICAL_PAGE_NO_WAIT);
	
	return 0;
}

void vm_translation_map_module_init_post_sem(kernel_args *ka)
{
}

int vm_translation_map_module_init2(kernel_args *ka)
{
	dprintf("vm_translation_map_module_init2: creating iospace region\n");
	void *temp = (void *)iospace_addr;
	vm_create_null_region(vm_get_kernel_aspace_id(), "iospace", &temp,
		REGION_ADDR_EXACT_ADDRESS, iospace_len);

	return 0;
}

// XXX horrible back door to map a page quickly regardless of translation map object, etc.
// used only during VM setup.
int vm_translation_map_quick_map(kernel_args *ka, addr_t va, addr_t pa, unsigned int attributes, addr_t (*get_free_page)(kernel_args *))
{
	addr_t pgtable_phys;
	unsigned long *pgtable;
	int index;

	dprintf("quick_map: va 0x%lx pa 0x%lx, attributes 0x%x\n", va, pa, attributes);

	// look up and dereference the first entry
	pgtable_phys = ka->arch_args.phys_pgdir;
	get_physical_page_tmap(pgtable_phys, (addr_t *)&pgtable, PHYSICAL_PAGE_NO_WAIT);
//	dprintf("phys 0x%lx, virt %p\n", pgtable_phys, pgtable);
	index = PGTABLE0_ENTRY(va);
	ASSERT(PGENT_PRESENT(pgtable[index]));

	// level 2
	pgtable_phys = PGENT_TO_ADDR(pgtable[index]);
	get_physical_page_tmap(pgtable_phys, (addr_t *)&pgtable, PHYSICAL_PAGE_NO_WAIT);
	index = PGTABLE1_ENTRY(va);
	if (!PGENT_PRESENT(pgtable[index])) {
		pgtable_phys = get_free_page(ka);
		pgtable[index] = pgtable_phys | 3;
		dprintf("had to allocate level 2: paddr 0x%lx\n", pgtable_phys);
	} else {
		pgtable_phys = PGENT_TO_ADDR(pgtable[index]);
//		dprintf("level 2: paddr 0x%lx\n", pgtable_phys);
	}

	// level 3
	get_physical_page_tmap(pgtable_phys, (addr_t *)&pgtable, PHYSICAL_PAGE_NO_WAIT);
	index = PGTABLE2_ENTRY(va);
	if (!PGENT_PRESENT(pgtable[index])) {
		pgtable_phys = get_free_page(ka);
		pgtable[index] = pgtable_phys | 3;
		dprintf("had to allocate level 3: paddr 0x%lx\n", pgtable_phys);
	} else {
		pgtable_phys = PGENT_TO_ADDR(pgtable[index]);
//		dprintf("level 3: paddr 0x%lx\n", pgtable_phys);
	}

	// map the page
	get_physical_page_tmap(pgtable_phys, (addr_t *)&pgtable, PHYSICAL_PAGE_NO_WAIT);
	index = PGTABLE3_ENTRY(va);
	pa = ROUNDOWN(pa, PAGE_SIZE);
	pgtable[index] = pa | ((attributes & LOCK_RW) ? (1<<1) : 0) | ((attributes & LOCK_KERNEL) ? 0 : (1<<2)) | 1;
	if (pa == 0x99d000) {
		dprintf("ent = 0x%lx\n", pgtable[index]);
		hexdump((void *)va, 16);
	}

	return 0;
}

// XXX currently assumes this translation map is active
static int vm_translation_map_quick_query(addr_t va, addr_t *out_physical)
{
	PANIC_UNIMPLEMENTED();
}

addr_t vm_translation_map_get_pgdir(vm_translation_map *map)
{
	return map->arch_data->pgdir_phys;
}
