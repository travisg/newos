/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_VM_H
#define _KERNEL_VM_H

#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/vfs.h>
#include <kernel/arch/vm_translation_map.h>

// vm page
typedef struct vm_page {
	struct vm_page *queue_prev;
	struct vm_page *queue_next;

	struct vm_page *hash_next;

	addr ppn; // physical page number
	off_t offset;

	struct vm_cache_ref *cache_ref;

	struct vm_page *cache_prev;
	struct vm_page *cache_next;

	unsigned int ref_count;

	unsigned int type : 2;
	unsigned int state : 3;
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
	mutex lock;

	struct vm_region *region_list;

	int ref_count;
} vm_cache_ref;

// vm_cache
typedef struct vm_cache {
	vm_page *page_list;
	vm_cache_ref *ref;
	struct vm_cache *source;
	struct vm_store *store;
	off_t virtual_size;
	unsigned int temporary : 1;
	unsigned int scan_skip : 1;
} vm_cache;

// info about a region that external entities may want to know
// used in vm_get_region_info()
typedef struct vm_region_info {
	region_id id;
	addr base;
	addr size;
	int lock;
	int wiring;
	char name[SYS_MAX_OS_NAME_LEN];
} vm_region_info;

// vm region
typedef struct vm_region {
	char *name;
	region_id id;
	addr base;
	addr size;
	int lock;
	int wiring;
	int ref_count;

	struct vm_cache_ref *cache_ref;
	off_t cache_offset;

	struct vm_address_space *aspace;
	struct vm_region *aspace_next;
	struct vm_virtual_map *map;
	struct vm_region *cache_next;
	struct vm_region *cache_prev;
	struct vm_region *hash_next;
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

enum {
	VM_ASPACE_STATE_NORMAL = 0,
	VM_ASPACE_STATE_DELETION
};

// address space
typedef struct vm_address_space {
	vm_virtual_map virtual_map;
	vm_translation_map translation_map;
	char *name;
	aspace_id id;
	int ref_count;
	int fault_count;
	int state;
	addr scan_va;
	addr working_set_size;
	addr max_working_set;
	addr min_working_set;
	bigtime_t last_working_set_adjust;
	struct vm_address_space *hash_next;
} vm_address_space;

// vm_store
typedef struct vm_store {
	struct vm_store_ops *ops;
	struct vm_cache *cache;
	void *data;
	off_t committed_size;
} vm_store;

// vm_store_ops
typedef struct vm_store_ops {
	void (*destroy)(struct vm_store *backing_store);
	off_t (*commit)(struct vm_store *backing_store, off_t size);
	int (*has_page)(struct vm_store *backing_store, off_t offset);
	ssize_t (*read)(struct vm_store *backing_store, off_t offset, iovecs *vecs);
	ssize_t (*write)(struct vm_store *backing_store, off_t offset, iovecs *vecs);
	int (*fault)(struct vm_store *backing_store, struct vm_address_space *aspace, off_t offset);
	void (*acquire_ref)(struct vm_store *backing_store);
	void (*release_ref)(struct vm_store *backing_store);
} vm_store_ops;

// args for the create_area funcs
enum {
	REGION_ADDR_ANY_ADDRESS = 0,
	REGION_ADDR_EXACT_ADDRESS
};

enum {
	REGION_NO_PRIVATE_MAP = 0,
	REGION_PRIVATE_MAP
};

enum {
	REGION_WIRING_LAZY = 0,
	REGION_WIRING_WIRED,
	REGION_WIRING_WIRED_ALREADY,
	REGION_WIRING_WIRED_CONTIG
 };

enum {
	PHYSICAL_PAGE_NO_WAIT = 0,
	PHYSICAL_PAGE_CAN_WAIT,
};

#define LOCK_RO        0x0
#define LOCK_RW        0x1
#define LOCK_KERNEL    0x2
#define LOCK_MASK      0x3

//void vm_dump_areas(vm_address_space *aspace);
int vm_init(kernel_args *ka);
int vm_init_postsem(kernel_args *ka);
int vm_init_postthread(kernel_args *ka);

aspace_id vm_create_aspace(const char *name, addr base, addr size, bool kernel);
int vm_delete_aspace(aspace_id);
vm_address_space *vm_get_kernel_aspace(void);
aspace_id vm_get_kernel_aspace_id(void);
vm_address_space *vm_get_current_user_aspace(void);
aspace_id vm_get_current_user_aspace_id(void);
vm_address_space *vm_get_aspace_by_id(aspace_id aid);
void vm_put_aspace(vm_address_space *aspace);
vm_region *vm_get_region_by_id(region_id rid);
void vm_put_region(vm_region *region);
#define vm_aspace_swap(aspace) arch_vm_aspace_swap(aspace)

region_id vm_create_anonymous_region(aspace_id aid, char *name, void **address, int addr_type,
	addr size, int wiring, int lock);
region_id vm_map_physical_memory(aspace_id aid, char *name, void **address, int addr_type,
	addr size, int lock, addr phys_addr);
region_id vm_map_file(aspace_id aid, char *name, void **address, int addr_type,
	addr size, int lock, int mapping, const char *path, off_t offset);
region_id vm_create_null_region(aspace_id aid, char *name, void **address, int addr_type, addr size);
region_id vm_clone_region(aspace_id aid, char *name, void **address, int addr_type,
	region_id source_region, int mapping, int lock);
int vm_delete_region(aspace_id aid, region_id id);
region_id vm_find_region_by_name(aspace_id aid, const char *name);
int vm_get_region_info(region_id id, vm_region_info *info);

int vm_get_page_mapping(aspace_id aid, addr vaddr, addr *paddr);
int vm_get_physical_page(addr paddr, addr *vaddr, int flags);
int vm_put_physical_page(addr vaddr);

int user_memcpy(void *to, const void *from, size_t size);
int user_strcpy(char *to, const char *from);
int user_strncpy(char *to, const char *from, size_t size);
int user_memset(void *s, char c, size_t count);

region_id user_vm_create_anonymous_region(char *uname, void **uaddress, int addr_type,
	addr size, int wiring, int lock);
region_id user_vm_clone_region(char *uname, void **uaddress, int addr_type,
	region_id source_region, int mapping, int lock);
region_id user_vm_map_file(char *uname, void **uaddress, int addr_type,
	addr size, int lock, int mapping, const char *upath, off_t offset);
int user_vm_get_region_info(region_id id, vm_region_info *uinfo);

// XXX remove later
void vm_test(void);

#endif

