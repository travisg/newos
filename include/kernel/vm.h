#ifndef _VM_H
#define _VM_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

// address space
struct aspace {
	struct aspace *next;
	aspace_id id;
	char *name;
	addr base;
	addr size;
	struct area *area_list;
	int area_count;
};	

// area
struct area {
	struct area *next;
	area_id id;
	char *name;
	addr base;
	addr size;
	int lock;
};

// args for the create_area funcs
enum {
	AREA_ANY_ADDRESS = 0,
	AREA_EXACT_ADDRESS,
	AREA_ALREADY_MAPPED
};

// flags for vm_create_area
#define AREA_NO_FLAGS  0
#define AREA_FLAGS_CONTIG    1

#define LOCK_RO        0
#define LOCK_RW        1
#define LOCK_KERNEL    2
#define LOCK_MASK      3

void vm_dump_areas(struct aspace *aspace);
int vm_init(kernel_args *ka);
int vm_init_postsem(kernel_args *ka);
struct aspace *vm_create_aspace(const char *name, unsigned int base, unsigned int size);
struct aspace *vm_get_kernel_aspace();
area_id vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock, int flags);
struct area *vm_find_area_by_name(struct aspace *aspace, const char *name);
int vm_map_physical_memory(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock, unsigned int phys_addr);
int vm_get_page_mapping(addr vaddr, addr *paddr);

#endif

