/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_VM_TRANSLATION_MAP_H
#define _NEWOS_KERNEL_ARCH_VM_TRANSLATION_MAP_H

#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/lock.h>
#include <kernel/list.h>

typedef struct vm_translation_map_struct {
	struct list_node tmap_list_node;
	struct vm_translation_map_ops_struct *ops;
	recursive_lock lock;
	int map_count;
	struct vm_translation_map_arch_info_struct *arch_data;
} vm_translation_map;

// table of operations the vm may want to do to this mapping
typedef struct vm_translation_map_ops_struct {
	void (*destroy)(vm_translation_map *);
	int (*lock)(vm_translation_map*);
	int (*unlock)(vm_translation_map*);
	int (*map)(vm_translation_map *map, addr_t va, addr_t pa, unsigned int attributes);
	int (*unmap)(vm_translation_map *map, addr_t start, addr_t end);
	int (*query)(vm_translation_map *map, addr_t va, addr_t *out_physical, unsigned int *out_flags);
	addr_t (*get_mapped_size)(vm_translation_map*);
	int (*protect)(vm_translation_map *map, addr_t base, addr_t top, unsigned int attributes);
	int (*clear_flags)(vm_translation_map *map, addr_t va, unsigned int flags);
	void (*flush)(vm_translation_map *map);
	int (*get_physical_page)(addr_t physical_address, addr_t *out_virtual_address, int flags);
	int (*put_physical_page)(addr_t virtual_address);
} vm_translation_map_ops;

int vm_translation_map_create(vm_translation_map *new_map, bool kernel);
int vm_translation_map_module_init(kernel_args *ka);
int vm_translation_map_module_init2(kernel_args *ka);
void vm_translation_map_module_init_post_sem(kernel_args *ka);
// quick function to map a page in regardless of map context. Used in VM initialization,
// before most vm data structures exist
int vm_translation_map_quick_map(kernel_args *ka, addr_t va, addr_t pa, unsigned int attributes, addr_t (*get_free_page)(kernel_args *));

// quick function to return the physical pgdir of a mapping, needed for a context switch
// XXX both are arch dependant
addr_t vm_translation_map_get_pgdir(vm_translation_map *map);	// x86
void vm_translation_map_change_asid(vm_translation_map *map);	// ppc


#endif

