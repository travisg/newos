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

// vm_translation object stuff
typedef struct vm_translation_map_arch_info_struct {
	// empty
} vm_translation_map_arch_info;

static int lock_tmap(vm_translation_map *map)
{
	dprintf("lock_tmap: map 0x%x\n", map);

	if(recursive_lock_lock(&map->lock) == true) {
		// we were the first one to grab the lock
	}

	return 0;
}

static int unlock_tmap(vm_translation_map *map)
{
	dprintf("unlock_tmap: map 0x%x\n", map);

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
	PANIC_UNIMPLEMENTED();
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

static int get_physical_page_tmap(addr_t pa, addr_t *va, int flags)
{
	PANIC_UNIMPLEMENTED();
}

static int put_physical_page_tmap(addr_t va)
{
	PANIC_UNIMPLEMENTED();
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
	PANIC_UNIMPLEMENTED();
}

int vm_translation_map_module_init(kernel_args *ka)
{
	int i;

	dprintf("vm_translation_map_module_init: entry\n");

	PANIC_UNIMPLEMENTED();

	return 0;
}


void vm_translation_map_module_init_post_sem(kernel_args *ka)
{
}

int vm_translation_map_module_init2(kernel_args *ka)
{
	dprintf("vm_translation_map_module_init2: entry\n");

	PANIC_UNIMPLEMENTED();

	return 0;
}

// XXX horrible back door to map a page quickly regardless of translation map object, etc.
// used only during VM setup.
// uses a 'page hole' set up in the stage 2 bootloader. The page hole is created by pointing one of
// the pgdir entries back at itself, effectively mapping the contents of all of the 4MB of pagetables
// into a 4 MB region. It's only used here, and is later unmapped.
int vm_translation_map_quick_map(kernel_args *ka, addr_t va, addr_t pa, unsigned int attributes, addr_t (*get_free_page)(kernel_args *))
{
	PANIC_UNIMPLEMENTED();
}

// XXX currently assumes this translation map is active
static int vm_translation_map_quick_query(addr_t va, addr_t *out_physical)
{
	PANIC_UNIMPLEMENTED();
}

addr_t vm_translation_map_get_pgdir(vm_translation_map *map)
{
	PANIC_UNIMPLEMENTED();
}
