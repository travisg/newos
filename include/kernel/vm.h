#ifndef _VM_H
#define _VM_H

#include <kernel.h>
#include <stage2.h>

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

#define LOCK_RO        0
#define LOCK_RW        1
#define LOCK_KERNEL    2
#define LOCK_MASK      3

void vm_dump_areas(struct aspace *aspace);
int vm_init(kernel_args *ka);
int vm_init_postsem(kernel_args *ka);
int vm_page_fault(int address, unsigned int fault_address);
int vm_get_free_page(unsigned int *page);
struct aspace *vm_create_aspace(const char *name, unsigned int base, unsigned int size);
struct aspace *vm_get_kernel_aspace();
area_id vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock);
struct area *vm_find_area_by_name(struct aspace *aspace, const char *name);
int vm_map_physical_memory(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock, unsigned int phys_addr);
void *kmalloc(unsigned int size);
void kfree(void *address);

// semi-private. Should only be used by vm internals
int vm_mark_page_inuse(unsigned int page);
int vm_mark_page_range_inuse(unsigned int start_page, unsigned int len);


#endif

