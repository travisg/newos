/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _VM_H
#define _VM_H

#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/arch/vm_translation_map.h>

// vm page
typedef struct vm_page {
	struct vm_page *queue_prev;
	struct vm_page *queue_next;

	addr ppn; // physical page number
	off_t offset;	
	
	struct vm_cache_ref *cache_ref;
	
	struct vm_page *cache_prev;
	struct vm_page *cache_next;
	
	unsigned int type : 2;
	unsigned int state : 3;
	unsigned int ref_count : 27;
} vm_page;

enum {
	PAGE_TYPE_PHYSICAL = 0,
	PAGE_TYPE_DUMMY,
	PAGE_TYPE_GUARD
};

enum {
	PAGE_STATE_ACTIVE = 0,
	PAGE_STATE_INACTIVE,
	PAGE_STATE_BUSY,
	PAGE_STATE_MODIFIED,
	PAGE_STATE_FREE,
	PAGE_STATE_CLEAR,
	PAGE_STATE_WIRED,
	PAGE_STATE_UNUSED
};

// vm_cache_ref
typedef struct vm_cache_ref {
	struct vm_cache *cache;
	sem_id sem;
	
	struct vm_region *region_list;

	int ref_count;
} vm_cache_ref;

// vm_cache
typedef struct vm_cache {
	vm_page *page_list;
	vm_cache_ref *ref;
	struct vm_store *store;
} vm_cache;

// vm region
typedef struct vm_region {
	char *name;
	region_id id;
	addr base;
	addr size;
	int lock;
	int wiring;
	
	struct vm_cache_ref *cache_ref;
	off_t cache_offset;	
	
	struct vm_region *next;
	struct vm_virtual_map *map;
	struct vm_region *cache_next;
	struct vm_region *cache_prev;
} vm_region;

// virtual map (1 per address space)
typedef struct vm_virtual_map {
	vm_region *region_list;
	vm_region *region_hint;
	int change_count;
	sem_id sem;
	struct vm_address_space *aspace;
	addr base;
	addr size;
} vm_virtual_map;

// address space
typedef struct vm_address_space {
	vm_virtual_map virtual_map;
	vm_translation_map translation_map;
	char *name;
} vm_address_space;	

// vm_store
typedef struct vm_store {
	struct vm_store_ops *ops;
	struct vm_cache *cache;
	void *data;
} vm_store;

// vm_store_ops
typedef struct vm_store_ops {
	void (*destroy)(struct vm_store *backing_store);
	off_t (*commit)(struct vm_store *backing_store, off_t size);
	int (*has_page)(struct vm_store *backing_store, off_t offset);
	int (*read)(struct vm_store *backing_store, off_t offset, void *buf, size_t *len);
	int (*write)(struct vm_store *backing_store, off_t offset, const void *buf, size_t *len);
	int (*fault)(struct vm_store *backing_store, struct vm_address_space *aspace, off_t offset);
} vm_store_ops;

#if 0
// area
struct area {
	struct area *next;
	area_id id;
	char *name;
	addr base;
	addr size;
	int lock;
};
#endif

// args for the create_area funcs
enum {
	REGION_ADDR_ANY_ADDRESS = 0,
	REGION_ADDR_EXACT_ADDRESS
};

enum {
	REGION_WIRING_LAZY = 0,
	REGION_WIRING_WIRED,
	REGION_WIRING_WIRED_ALREADY,
	REGION_WIRING_WIRED_PHYSICAL
};

#define LOCK_RO        0x0
#define LOCK_RW        0x1
#define LOCK_KERNEL    0x2
#define LOCK_MASK      0x3

//void vm_dump_areas(vm_address_space *aspace);
int vm_init(kernel_args *ka);
int vm_init_postsem(kernel_args *ka);
vm_address_space *vm_create_aspace(const char *name, unsigned int base, unsigned int size, bool kernel);
vm_address_space *vm_get_kernel_aspace();
vm_address_space *vm_get_current_user_aspace();
vm_region *vm_create_anonymous_region(vm_address_space *aspace, char *name, void **address, int addr_type,
	addr size, int wiring, int lock);
vm_region *vm_map_physical_memory(vm_address_space *aspace, char *name, void **address, int addr_type,
	addr size, int lock, addr phys_addr);
vm_region *vm_find_region_by_name(vm_address_space *aspace, const char *name);
int vm_get_page_mapping(vm_address_space *aspace, addr vaddr, addr *paddr);

#endif

