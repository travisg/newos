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
	int magic;
	struct vm_page *queue_prev;
	struct vm_page *queue_next;

	struct vm_page *hash_next;

	off_t offset;
	addr_t ppn; // physical page number

	struct vm_cache_ref *cache_ref;

	struct vm_page *cache_prev;
	struct vm_page *cache_next;

	unsigned int ref_count;

	unsigned int type : 2;
	unsigned int state : 3;
} vm_page;

#define VM_PAGE_MAGIC 'vmpg'

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
	int magic;
	struct vm_cache *cache;
	mutex lock;

	struct vm_region *region_list;

	int ref_count;
} vm_cache_ref;

#define VM_CACHE_REF_MAGIC 'vmcr'

// vm_cache
typedef struct vm_cache {
	int magic;
	vm_page *page_list;
	vm_cache_ref *ref;
	struct vm_cache *source;
	struct vm_store *store;
	unsigned int temporary : 1;
	unsigned int scan_skip : 1;
	off_t virtual_size;
} vm_cache;

#define VM_CACHE_MAGIC 'vmca'

// info about a region that external entities may want to know
// used in vm_get_region_info()
typedef struct vm_region_info {
	region_id id;
	addr_t base;
	addr_t size;
	int lock;
	int wiring;
	char name[SYS_MAX_OS_NAME_LEN];
} vm_region_info;

// vm region
typedef struct vm_region {
	int magic;
	char *name;
	region_id id;
	addr_t base;
	addr_t size;
	int lock;
	int wiring;
	int ref_count;

	off_t cache_offset;
	struct vm_cache_ref *cache_ref;

	struct vm_address_space *aspace;
	struct vm_region *aspace_next;
	struct vm_virtual_map *map;
	struct vm_region *cache_next;
	struct vm_region *cache_prev;
	struct vm_region *hash_next;
} vm_region;

#define VM_REGION_MAGIC 'vmrg'

// virtual map (1 per address space)
typedef struct vm_virtual_map {
	vm_region *region_list;
	vm_region *region_hint;
	int change_count;
	sem_id sem;
	struct vm_address_space *aspace;
	addr_t base;
	addr_t size;
} vm_virtual_map;

enum {
	VM_ASPACE_STATE_NORMAL = 0,
	VM_ASPACE_STATE_DELETION
};

// address space
typedef struct vm_address_space {
	int magic;
	vm_virtual_map virtual_map;
	vm_translation_map translation_map;
	char *name;
	aspace_id id;
	int ref_count;
	int fault_count;
	int state;
	addr_t scan_va;
	addr_t working_set_size;
	addr_t max_working_set;
	addr_t min_working_set;
	bigtime_t last_working_set_adjust;
	struct vm_address_space *hash_next;
} vm_address_space;

#define VM_ASPACE_MAGIC 'vmas'

// vm_store
typedef struct vm_store {
	int magic;
	struct vm_store_ops *ops;
	struct vm_cache *cache;
	void *data;
	off_t committed_size;
} vm_store;

#define VM_STORE_MAGIC 'vmst'

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

aspace_id vm_create_aspace(const char *name, addr_t base, addr_t size, bool kernel);
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
	addr_t size, int wiring, int lock);
region_id vm_map_physical_memory(aspace_id aid, char *name, void **address, int addr_type,
	addr_t size, int lock, addr_t phys_addr);
region_id vm_map_file(aspace_id aid, char *name, void **address, int addr_type,
	addr_t size, int lock, int mapping, const char *path, off_t offset);
region_id vm_create_null_region(aspace_id aid, char *name, void **address, int addr_type, addr_t size);
region_id vm_clone_region(aspace_id aid, char *name, void **address, int addr_type,
	region_id source_region, int mapping, int lock);
int vm_delete_region(aspace_id aid, region_id id);
region_id vm_find_region_by_name(aspace_id aid, const char *name);
int vm_get_region_info(region_id id, vm_region_info *info);

int vm_get_page_mapping(aspace_id aid, addr_t vaddr, addr_t *paddr);
int vm_get_physical_page(addr_t paddr, addr_t *vaddr, int flags);
int vm_put_physical_page(addr_t vaddr);

int user_memcpy(void *to, const void *from, size_t size);
int user_strcpy(char *to, const char *from);
int user_strncpy(char *to, const char *from, size_t size);
int user_memset(void *s, char c, size_t count);

region_id user_vm_create_anonymous_region(char *uname, void **uaddress, int addr_type,
	addr_t size, int wiring, int lock);
region_id user_vm_clone_region(char *uname, void **uaddress, int addr_type,
	region_id source_region, int mapping, int lock);
region_id user_vm_map_file(char *uname, void **uaddress, int addr_type,
	addr_t size, int lock, int mapping, const char *upath, off_t offset);
int user_vm_get_region_info(region_id id, vm_region_info *uinfo);

// state of the vm, for informational purposes only
typedef struct {
	// info about the size of memory in the system
	int physical_page_size;
	int physical_pages;

	// amount of committed mem in the system
	int committed_mem;

	// info about the page queues
	int active_pages;
	int inactive_pages;
	int busy_pages;
	int unused_pages;
	int modified_pages;
	int free_pages;
	int clear_pages;
	int wired_pages;

	// info about vm activity
	int page_faults;
} vm_info_t;

addr_t vm_get_mem_size(void);

// XXX remove later
void vm_test(void);

#endif

