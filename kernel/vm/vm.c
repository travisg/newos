/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/vm_page.h>
#include <kernel/vm_cache.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/sem.h>

#include <boot/stage2.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/vm.h>

#include <libc/string.h>
#include <libc/ctype.h>
#include <libc/printf.h>

#define HEAP_SIZE	0x00400000

#define WRITE_COUNT 1024
#define READ_COUNT 1

static vm_address_space *kernel_aspace;

static region_id next_region_id;

//static sem_id region_list_sem;

// function declarations
vm_region *_vm_create_region_struct(vm_address_space *aspace, char *name, addr base,
	addr size, int wiring, int lock);
vm_region *_vm_create_region(vm_address_space *aspace, char *name, void **address,
	int addr_type, addr size, int wiring, int lock);
int vm_soft_fault(addr address, bool is_write, bool is_user);
vm_region *vm_virtual_map_lookup(vm_virtual_map *map, addr address);

vm_region *vm_find_region_by_name(vm_address_space *aspace, const char *name)
{
	vm_region *region = NULL;

	sem_acquire(aspace->virtual_map.sem, READ_COUNT);

	region = aspace->virtual_map.region_list;
	while(region != NULL) {
		if(strcmp(region->name, name) == 0)
			break;
		region = region->next;
	}

	sem_release(aspace->virtual_map.sem, READ_COUNT);
	return region;
}

// creates semi-initialized region struct and adds to aspace's region list
// NOTE: region_list_sem must be held
vm_region *_vm_create_region_struct(vm_address_space *aspace, char *name, addr base,
	addr size, int wiring, int lock)
{
	vm_region *region;
	vm_region *a, *last = NULL;
	
//	dprintf("_vm_create_region_struct: aspace 0x%x name '%s' base 0x%x, size 0x%x\n", aspace, name, base, size);
	
	// allocate a region struct to represent this region
	region = (vm_region *)kmalloc(sizeof(vm_region));	
	region->base = base;
	region->size = size;
	region->id = next_region_id++;
	region->lock = lock;
	region->wiring = wiring;
	region->map = &aspace->virtual_map;
	region->name = (char *)kmalloc(strlen(name) + 1);
	if(region->name == NULL) {
		kfree(region);
		return NULL;
	}
	strcpy(region->name, name);
	
	region->cache_ref = NULL;
	region->cache_offset = 0;
	region->cache_next = NULL;
	region->cache_prev = NULL;

	// insert into the list
	// we'll need to search for the spot
	// check for address overlaps
	a = aspace->virtual_map.region_list;
	while(a != NULL) {
//		dprintf("_vm_create_region_struct: a = 0x%x. base = 0x%x, size = 0x%x\n", a, a->base, a->size);
		if(a->base > base) {
			if(base + size > a->base) {
				// overlap
				kfree(region->name);
				kfree(region);
				return NULL;
			}
			region->next = a;
			if(last == NULL)
				aspace->virtual_map.region_list = region;
			else
				last->next = region;
			break;
		}
		last = a;
		a = a->next;
	}
	if(a == NULL) {
		region->next = NULL;
		if(last == NULL)
			aspace->virtual_map.region_list = region;
		else
			last->next = region;
	}
//	dprintf("_vm_create_region_struct: returning 0x%x\n", region);
	return region;
}

// finds a place for a region in the address space and creates the initial structure
vm_region *_vm_create_region(vm_address_space *aspace, char *name, void **address,
	int addr_type, addr size, int wiring, int lock)
{
	vm_region *region;
	unsigned int base;

	dprintf("_vm_create_region: '%s', *address = 0x%p, addr_type = %d, size = %d\n",
		name, *address, addr_type, size);

	// check validity of lock
	if((lock & ~LOCK_MASK) != 0) {
		// invalid lock
		panic("_vm_create_region called with invalid lock %d\n", lock);
	}

	sem_acquire(aspace->virtual_map.sem, WRITE_COUNT);

	region = NULL;
	switch(addr_type) {
		case REGION_ADDR_ANY_ADDRESS: {
			vm_region *r;
			vm_region *next_r = NULL;
			// find a hole big enough for a new region
	
			base = 0;
	
			r = aspace->virtual_map.region_list;
			if(r == NULL) {
				base = aspace->virtual_map.base;
			} else if(r != NULL && r->base > aspace->virtual_map.base) {
				// lets try to build the region at the beginning of the aspace
				if(aspace->virtual_map.base + size > r->base) {
					// if we built it here, it would overlap, so let the loop below
					// find the right spot
					next_r = r->next;
				} else {
					// otherwise, we're done.
					base = aspace->virtual_map.base;
					r = NULL;
				}
			} else {
				next_r = r->next;
			}
			while(r != NULL) {
//				dprintf("r = 0x%x. base = 0x%x, size = 0x%x\n", r, r->base, r->size);
				if(next_r != NULL) {
//					dprintf("next_r = 0x%x. base = 0x%x, size = 0x%x\n", next_r, next_r->base, next_r->size);
					if(next_r->base - (r->base + r->size) >= size) {
						// we have a spot
						base = r->base + r->size;
						break;
					}
				} else {
					if((aspace->virtual_map.base + aspace->virtual_map.size) - (r->base + r->size) + 1 >= size) {
						// we have a spot
						base = r->base + r->size;
						break;
					}
				}
				r = next_r;
				if(next_r != NULL)
					next_r = next_r->next;
			}
			
			if(base == 0)
				break; // didn't find a spot

			*address = (void *)base;
			region = _vm_create_region_struct(aspace, name, base, size, wiring, lock);
			break;
		}
		case REGION_ADDR_EXACT_ADDRESS:
			base = (unsigned int)*address;

			if(base < aspace->virtual_map.base || (base - aspace->virtual_map.base) + size > (base - aspace->virtual_map.base) + aspace->virtual_map.size) {
				dprintf("_vm_create_region: region asked to be created outside of aspace\n");
				break;
			}

			region = _vm_create_region_struct(aspace, name, base, size, wiring, lock);
			break;
		default:
			// whut?
			panic("_vm_create_region: invalid addr_type flag passed %d\n", addr_type);
	}

	sem_release(aspace->virtual_map.sem, WRITE_COUNT);

	return region;
}

static vm_region *_vm_create_anonymous_region(vm_address_space *aspace, char *name, void **address, int addr_type,
	addr size, int wiring, int lock, addr phys_addr)
{
	vm_region *region;
	vm_cache *cache;
	vm_cache_ref *cache_ref;
	
	region = _vm_create_region(aspace, name, address, addr_type, size, wiring, lock);
	if(region == NULL)
		return NULL;

	// create a new cache

	// null vm_store for this one
	cache = vm_cache_create(NULL);
	if(cache == NULL)
		panic("vm_create_anonymous_region: vm_cache_create returned NULL");
	cache_ref = vm_cache_ref_create(cache);
	if(cache_ref == NULL)
		panic("vm_create_anonymous_region: vm_cache_ref_create returned NULL");

	vm_cache_acquire_ref(cache_ref);
	
	region->cache_ref = cache_ref;
	region->cache_offset = 0;

	switch(wiring) {
		case REGION_WIRING_LAZY:
			break; // do nothing
		case REGION_WIRING_WIRED: {
			// pages aren't mapped at this point, but we just simulate a fault on
			// every page, which should allocate them
			addr va;
			// XXX remove
			for(va = region->base; va < region->base + region->size; va += PAGE_SIZE) {
				dprintf("mapping wired pages: region 0x%x, cache_ref 0x%x 0x%x\n", region, cache_ref, region->cache_ref);
				vm_soft_fault(va, false, false);
			}
			break;
		}
		case REGION_WIRING_WIRED_ALREADY: {
			// the pages should already be mapped. This is only really useful during
			// boot time. Find the appropriate vm_page objects and stick them in
			// the cache object.
			addr va;
			addr pa;
			unsigned int flags;
			int err;
			vm_page *page;
			off_t offset = 0;

			sem_acquire(cache_ref->sem, 1);
			(*aspace->translation_map.ops->lock)(&aspace->translation_map);
			for(va = region->base; va < region->base + region->size; va += PAGE_SIZE, offset += PAGE_SIZE) {
				err = (*aspace->translation_map.ops->query)(&aspace->translation_map,
					va, &pa, &flags);
				if(err < 0) {
					dprintf("vm_create_anonymous_region: error looking up mapping for va 0x%x\n", va);
					continue;
				}
				page = vm_lookup_page(pa / PAGE_SIZE);
				if(page == NULL) {
					dprintf("vm_create_anonymous_region: error looking up vm_page structure for pa 0x%x\n", pa);
					continue;
				}
				vm_page_set_state(page, PAGE_STATE_WIRED);
				vm_cache_insert_page(cache_ref, page, offset);
			}
			(*aspace->translation_map.ops->unlock)(&aspace->translation_map);
			sem_release(cache_ref->sem, 1);
			break;
		}
		case REGION_WIRING_WIRED_PHYSICAL: {
			addr va;
			unsigned int flags;
			int err;
			vm_page *page;
			off_t offset = 0;

			sem_acquire(cache_ref->sem, 1);
			(*aspace->translation_map.ops->lock)(&aspace->translation_map);
			for(va = region->base; va < region->base + region->size; va += PAGE_SIZE, offset += PAGE_SIZE, phys_addr += PAGE_SIZE) {
				err = (*aspace->translation_map.ops->map)(&aspace->translation_map,
					va, phys_addr, lock);
				if(err < 0) {
					dprintf("vm_create_anonymous_region: error mapping va 0x%x to pa 0x%x\n",
						va, phys_addr);
				}
				page = vm_page_allocate_specific_page(phys_addr / PAGE_SIZE, PAGE_STATE_FREE);
				if(page == NULL) {
					// not finding a page structure is not the end of the world
					// this may be covering a range of the physical address space
					// that is not covered by RAM, such as a device.
					continue;
				}
				vm_page_set_state(page, PAGE_STATE_WIRED);
				vm_cache_insert_page(cache_ref, page, offset);
			}
			(*aspace->translation_map.ops->unlock)(&aspace->translation_map);
			sem_release(cache_ref->sem, 1);
			break;
		}
		case REGION_WIRING_WIRED_CONTIG: {
			addr va;
			addr phys_addr; 
			int err;
			vm_page *page;
			off_t offset = 0;
	
			page = vm_page_allocate_page_run(PAGE_STATE_CLEAR, ROUNDUP(region->size, PAGE_SIZE) / PAGE_SIZE);
			if(page == NULL) {
				// XXX back out of this
				panic("couldn't allocate page run of size %d\n", region->size);
			}
			phys_addr = page->ppn * PAGE_SIZE;

			sem_acquire(cache_ref->sem, 1);
			(*aspace->translation_map.ops->lock)(&aspace->translation_map);
			for(va = region->base; va < region->base + region->size; va += PAGE_SIZE, offset += PAGE_SIZE, phys_addr += PAGE_SIZE) {
				page = vm_lookup_page(phys_addr / PAGE_SIZE);
				if(page == NULL) {
					panic("couldn't lookup physical page just allocated\n");
				}
				err = (*aspace->translation_map.ops->map)(&aspace->translation_map, va, phys_addr, lock);
				if(err < 0) {
					panic("couldn't map physical page in page run\n");
				}
				vm_page_set_state(page, PAGE_STATE_WIRED);
				vm_cache_insert_page(cache_ref, page, offset);
			}
			(*aspace->translation_map.ops->unlock)(&aspace->translation_map);
			sem_release(cache_ref->sem, 1);

			break;
		}
		default:
			;		
	}
	return region;
}

vm_region *vm_create_anonymous_region(vm_address_space *aspace, char *name, void **address, int addr_type,
	addr size, int wiring, int lock)
{
	if(wiring == REGION_WIRING_WIRED_PHYSICAL) {
		// invalid here
		return NULL;
	}
	return _vm_create_anonymous_region(aspace, name, address, addr_type, size, wiring, lock, 0);
}

vm_region *vm_map_physical_memory(vm_address_space *aspace, char *name, void **address, int addr_type,
	addr size, int lock, addr phys_addr)
{
	return _vm_create_anonymous_region(aspace, name, address, addr_type, size,
		REGION_WIRING_WIRED_PHYSICAL, lock, phys_addr);
}

#if 0	
vm_region *vm_create_region(vm_address_space *aspace, char *name, void **address, int addr_type,
	unsigned int size, unsigned int lock, int flags)
{
	if(addr_type == AREA_ALREADY_MAPPED) {
		return _vm_create_region(aspace, name, address, REGION_ADDR_EXACT_ADDRESS, size, lock, 0, SRC_ADDR_MAPPED_ALREADY);
	} else {
		if(flags == AREA_FLAGS_CONTIG)
			return _vm_create_region(aspace, name, address, addr_type, size, lock, 0, SRC_ADDR_CONTIGUOUS);
		else
			return _vm_create_region(aspace, name, address, addr_type, size, lock, 0, 0);
	}

}

vm_region *vm_map_physical_memory(vm_address_space *aspace, char *name, void **address, int addr_type,
	unsigned int size, unsigned int lock, unsigned int phys_addr)
{
	return _vm_create_region(aspace, name, address, addr_type, size, lock, phys_addr, SRC_ADDR_PHYSICAL);
}
#endif

int vm_get_page_mapping(vm_address_space *aspace, addr vaddr, addr *paddr)
{
	unsigned int null_flags;
	return aspace->translation_map.ops->query(&aspace->translation_map,
		vaddr, paddr, &null_flags);
}

static void display_mem(int argc, char **argv)
{
	int item_size;
	int display_width;
	int num = 1;
	addr address;
	int i;
	int j;
	
	if(argc < 2) {
		dprintf("not enough arguments\n");
		return;
	}
	
	address = atoul(argv[1]);
	
	if(argc >= 3) {
		num = -1;
		num = atoi(argv[2]);
	}

	// build the format string
	if(strcmp(argv[0], "db") == 0) {
		item_size = 1;
		display_width = 16;
	} else if(strcmp(argv[0], "ds") == 0) {
		item_size = 2;
		display_width = 8;
	} else if(strcmp(argv[0], "dw") == 0) {
		item_size = 4;
		display_width = 4;
	} else {
		dprintf("display_mem called in an invalid way!\n");
		return;
	}

	dprintf("[0x%x] '", address);
	for(j=0; j<min(display_width, num) * item_size; j++) {
		char c = *((char *)address + j);
		if(!isalnum(c)) {
			c = '.';
		}
		dprintf("%c", c);
	}
	dprintf("'");
	for(i=0; i<num; i++) {	
		if((i % display_width) == 0 && i != 0) {
			dprintf("\n[0x%x] '", address + i * item_size);
			for(j=0; j<min(display_width, (num-i)) * item_size; j++) {
				char c = *((char *)address + i * item_size + j);
				if(!isalnum(c)) {
					c = '.';
				}
				dprintf("%c", c);
			}
			dprintf("'");
		}
		
		switch(item_size) {
			case 1:
				dprintf(" 0x%02x", *((uint8 *)address + i));
				break;
			case 2:
				dprintf(" 0x%04x", *((uint16 *)address + i));
				break;
			case 4:
				dprintf(" 0x%08x", *((uint32 *)address + i));
				break;
			default:
				dprintf("huh?\n");
		}
	}
	dprintf("\n");
}

void vm_dump_regions(vm_address_space *aspace)
{
	vm_region *region;

	dprintf("region dump of address space '%s', base 0x%x, size 0x%x:\n", aspace->name, aspace->virtual_map.base, aspace->virtual_map.size);

	for(region = aspace->virtual_map.region_list; region != NULL; region = region->next) {
		dprintf("region 0x%x: ", region->id);
		dprintf("base_addr = 0x%x ", region->base);
		dprintf("size = 0x%x ", region->size);
		dprintf("name = '%s' ", region->name);
		dprintf("lock = 0x%x\n", region->lock);
	}
}

static void vm_dump_kspace_regions(int argc, char **argv)
{
	vm_dump_regions(vm_get_kernel_aspace());
}

#if 0
static void dump_aspace_regions(int argc, char **argv)
{
	int id = -1;
	unsigned long num;
	vm_address_space *as;

	if(argc < 2) {
		dprintf("aspace_regions: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		num = atoul(argv[1]);
		id = num;
	}

	for(as = aspace_list; as != NULL; as = as->next) {
		if(as->id == id || !strcmp(argv[1], as->name)) {
			vm_dump_regions(as);
			break;
		}
	}
}

static void dump_aspace_list(int argc, char **argv)
{
	vm_address_space *as;

	dprintf("id\t%32s\tbase\tsize\t\tregion_count\n", "name");
	for(as = aspace_list; as != NULL; as = as->next) {
		dprintf("0x%x\t%32s\t0x%x\t0x%x\t0x%x\n",
			as->id, as->name, as->base, as->size, as->region_count);
	}
}
#endif

vm_address_space *vm_get_kernel_aspace()
{
	return kernel_aspace;
}

vm_address_space *vm_get_current_user_aspace()
{
	return thread_get_current_thread()->proc->aspace;
}

vm_address_space *vm_create_aspace(const char *name, unsigned int base, unsigned int size, bool kernel)
{
	vm_address_space *aspace;
	int err;
	
	aspace = (vm_address_space *)kmalloc(sizeof(vm_address_space));
	if(aspace == NULL)
		return NULL;

	aspace->name = (char *)kmalloc(strlen(name) + 1);
	if(aspace->name == NULL ) {
		kfree(aspace);
		return NULL;
	}
	strcpy(aspace->name, name);

	// initialize the corresponding translation map
	err = vm_translation_map_create(&aspace->translation_map, kernel);
	if(err < 0) {
		kfree(aspace->name);
		kfree(aspace);
		return NULL;
	}

	// initialize the virtual map
	aspace->virtual_map.base = base;
	aspace->virtual_map.size = size;
	aspace->virtual_map.region_list = NULL;
	aspace->virtual_map.region_hint = NULL;
	aspace->virtual_map.change_count = 0;
	aspace->virtual_map.sem = sem_create(WRITE_COUNT, "aspacelock");
	aspace->virtual_map.aspace = aspace;

	return aspace;	
}

void vm_test()
{
	vm_region *region;
	addr region_addr;
	int i;
	
	dprintf("vm_test: entry\n");
	
	region = vm_create_anonymous_region(vm_get_kernel_aspace(), "test_region", (void **)&region_addr,
		REGION_ADDR_ANY_ADDRESS, PAGE_SIZE * 16, REGION_WIRING_LAZY, LOCK_RW|LOCK_KERNEL);
	dprintf("region = 0x%x, addr = 0x%x, region->base = 0x%x\n", region, region_addr, region->base);

	memset((void *)region_addr, 0, PAGE_SIZE * 16);
	
	region = vm_map_physical_memory(vm_get_kernel_aspace(), "test_physical_region", (void **)&region_addr,
		REGION_ADDR_ANY_ADDRESS, PAGE_SIZE * 16, LOCK_RW|LOCK_KERNEL, 0xb8000);
	dprintf("region = 0x%x, addr = 0x%x, region->base = 0x%x\n", region, region_addr, region->base);

	for(i=0; i<64; i++) {
		((char *)region_addr)[i] = 'a';
	}	

	region = vm_create_anonymous_region(vm_get_kernel_aspace(), "test_region_wired", (void **)&region_addr,
		REGION_ADDR_ANY_ADDRESS, PAGE_SIZE * 16, REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
	dprintf("region = 0x%x, addr = 0x%x, region->base = 0x%x\n", region, region_addr, region->base);

	memset((void *)region_addr, 0, PAGE_SIZE * 16);
	
	dprintf("vm_test: done\n");
}

int vm_init(kernel_args *ka)
{
	int err = 0;
	unsigned int i;
	int last_used_virt_range = -1;
	int last_used_phys_range = -1;
	addr heap_base;
	void *null_addr;

	dprintf("vm_init: entry\n");
	err = vm_translation_map_module_init(ka);
	err = arch_vm_init(ka);

	// initialize some globals
	kernel_aspace = NULL;
	next_region_id = 0;
	
	// map in the new heap and initialize it
	heap_base = vm_alloc_from_ka_struct(ka, HEAP_SIZE, LOCK_KERNEL|LOCK_RW);
	dprintf("heap at 0x%x\n", heap_base);
	heap_init(heap_base, HEAP_SIZE);

	// initialize the free page list
	vm_page_init(ka);
	
	// create the initial kernel address space
	kernel_aspace = vm_create_aspace("kernel_land", KERNEL_BASE, KERNEL_SIZE, true);

	// do any further initialization that the architecture dependant layers may need now
	vm_translation_map_module_init2(ka);
	arch_vm_init2(ka);
	vm_page_init2(ka);

	// allocate regions to represent stuff that already exists
	null_addr = (void *)ROUNDOWN(heap_base, PAGE_SIZE);
	vm_create_anonymous_region(kernel_aspace, "kernel_heap", &null_addr, REGION_ADDR_EXACT_ADDRESS,
		HEAP_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	null_addr = (void *)ROUNDOWN(ka->kernel_seg0_addr.start, PAGE_SIZE);
	vm_create_anonymous_region(kernel_aspace, "kernel_seg0", &null_addr, REGION_ADDR_EXACT_ADDRESS,
		PAGE_ALIGN(ka->kernel_seg0_addr.size), REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	if(ka->kernel_seg1_addr.size > 0) {
		null_addr = (void *)ROUNDOWN(ka->kernel_seg1_addr.start, PAGE_SIZE);
		vm_create_anonymous_region(kernel_aspace, "kernel_seg1", &null_addr, REGION_ADDR_EXACT_ADDRESS,
			PAGE_ALIGN(ka->kernel_seg1_addr.size), REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
	}
	for(i=0; i < ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d_kstack", i);
		null_addr = (void *)ka->cpu_kstack[i].start;
		vm_create_anonymous_region(kernel_aspace, temp, &null_addr, REGION_ADDR_EXACT_ADDRESS,
			ka->cpu_kstack[i].size, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
	}
	{
		void *null;
		vm_map_physical_memory(kernel_aspace, "bootdir", &null, REGION_ADDR_ANY_ADDRESS,
			ka->bootdir_addr.size, LOCK_RO|LOCK_KERNEL, ka->bootdir_addr.start);
	}
	vm_dump_regions(kernel_aspace);

	// add some debugger commands
//	dbg_add_command(&dump_aspace_regions, "aspace_regions", "Dump regions in an address space");
//	dbg_add_command(&dump_aspace_list, "aspaces", "Dump a list of all address spaces");
	dbg_add_command(&vm_dump_kspace_regions, "region_dump_kspace", "Dump kernel space regions");
//	dbg_add_command(&display_mem, "dl", "dump memory long words (64-bit)");
	dbg_add_command(&display_mem, "dw", "dump memory words (32-bit)");
	dbg_add_command(&display_mem, "ds", "dump memory shorts (16-bit)");
	dbg_add_command(&display_mem, "db", "dump memory bytes (8-bit)");

	dprintf("vm_init: exit\n");
	
	vm_test();
//	panic("done with tests\n");

	return err;
}

int vm_init_postsem(kernel_args *ka)
{
	vm_region *region;
	
	// fill in all of the semaphores that were not allocated before
	// since we're still single threaded and only the kernel address space exists,
	// it isn't that hard to find all of the ones we need to create
	kernel_aspace->virtual_map.sem = sem_create(WRITE_COUNT, "kernel_aspacelock");
	
	for(region = kernel_aspace->virtual_map.region_list; region; region = region->next) {
		if(region->cache_ref->sem < 0) {
			region->cache_ref->sem = sem_create(1, "cache_ref_sem");
		}
	}

	return heap_init_postsem(ka);
}

int vm_page_fault(addr address, addr fault_address, bool is_write, bool is_user)
{
	int err;
	
	dprintf("vm_page_fault: page fault at 0x%x, ip 0x%x\n", address, fault_address);
	
	err = vm_soft_fault(address, is_write, is_user);
	if(err < 0) {
		if(!is_user) {
			// unhandled page fault in the kernel
			panic("vm_page_fault: unhandled page fault in kernel space at 0x%x, ip 0x%x\n",
				address, fault_address);
		} else {
			// XXX kill the process
		}			
	}

	return INT_NO_RESCHEDULE;
}

int vm_soft_fault(addr address, bool is_write, bool is_user)
{
	vm_address_space *aspace;
	vm_virtual_map *map;
	vm_region *region;
	vm_cache_ref *cache_ref;
	off_t cache_offset;
	vm_page dummy_page;
	vm_page *page;
	int change_count;
	int err;

	dprintf("vm_soft_fault: address 0x%x, is_write %d, is_user %d\n",
		address, is_write, is_user);
	
	address = ROUNDOWN(address, PAGE_SIZE);
	
	if(address >= KERNEL_BASE && address <= KERNEL_BASE + (KERNEL_SIZE - 1)) {
		aspace = vm_get_kernel_aspace();
	} else {
		aspace = vm_get_current_user_aspace();
	}
	map = &aspace->virtual_map;
	
	sem_acquire(map->sem, READ_COUNT);
	region = vm_virtual_map_lookup(map, address);
	if(region == NULL) {
		sem_release(map->sem, READ_COUNT);
		dprintf("vm_soft_fault: va 0x%x not covered by region in address space\n", address);
		return -1; // BAD_ADDRESS
	}
	// XXX check permissions
	
	cache_ref = region->cache_ref;
	cache_offset = address - region->base + region->cache_offset;
	vm_cache_acquire_ref(cache_ref);
	change_count = map->change_count;
	sem_release(map->sem, READ_COUNT);

	// see if this cache has a fault handler
	if(cache_ref->cache->store && cache_ref->cache->store->ops->fault) {
		int err = (*cache_ref->cache->store->ops->fault)(cache_ref->cache->store, aspace, cache_offset);
		vm_cache_release_ref(cache_ref);
		return err;
	}
	
	dummy_page.state = PAGE_STATE_INACTIVE;
	
	sem_acquire(cache_ref->sem, 1);

	for(;;) {
		 page = vm_cache_lookup_page(cache_ref, cache_offset);
		 if(page != NULL && page->state != PAGE_STATE_BUSY) {
		 	vm_page_set_state(page, PAGE_STATE_BUSY);
		 	sem_release(cache_ref->sem, 1);
		 	break;
		 }
		 
		 if(page == NULL)
		 	break;
		 
		 // page must be busy
		 sem_release(cache_ref->sem, 1);
		 thread_snooze(20000);
		 sem_acquire(cache_ref->sem, 1);
	}

	if(page == NULL) {
		// still havent found a page, insert the dummy page to lock it
		dummy_page.state = PAGE_STATE_BUSY;
		vm_cache_insert_page(cache_ref, &dummy_page, cache_offset);
		
		// see if the vm_store has it
		if(cache_ref->cache->store && cache_ref->cache->store->ops->has_page) {
			if(cache_ref->cache->store->ops->has_page(cache_ref->cache->store, cache_offset)) {
				// XXX read it in from the store
				panic("not done\n");
			}
		}
		sem_release(cache_ref->sem, 1);
	}

	if(page == NULL) {
		// still haven't found a page, so zero out a new one
		page = vm_page_allocate_page(PAGE_STATE_CLEAR);
		dprintf("vm_soft_fault: just allocated page 0x%x\n", page->ppn);
		sem_acquire(cache_ref->sem, 1);
		vm_cache_remove_page(cache_ref, &dummy_page);
		vm_cache_insert_page(cache_ref, page, cache_offset);
		sem_release(cache_ref->sem, 1);
		dummy_page.state = PAGE_STATE_INACTIVE;
	}

	if(page->cache_ref != cache_ref && is_write) {
		// XXX handle making private copy on write
		panic("not done\n");
	}
	
	err = 0;
	sem_acquire(map->sem, READ_COUNT);
	if(change_count != map->change_count) {
		// something may have changed, see if the address is still valid
		region = vm_virtual_map_lookup(map, address);
		if(region == NULL || region->cache_ref != cache_ref || region->cache_offset != cache_offset) {
			dprintf("vm_soft_fault: address space layout changed effecting ongoing soft fault\n");
			err = -1; // BAD_ADDRESS
		}
	}

	if(err == 0) {
		(*aspace->translation_map.ops->lock)(&aspace->translation_map);
		(*aspace->translation_map.ops->map)(&aspace->translation_map, address,
			page->ppn * PAGE_SIZE, region->lock);
		(*aspace->translation_map.ops->unlock)(&aspace->translation_map);
	}
	
	sem_release(map->sem, READ_COUNT);

	if(dummy_page.state == PAGE_STATE_BUSY) {
		sem_acquire(cache_ref->sem, 1);
		vm_cache_remove_page(cache_ref, &dummy_page);
		sem_release(cache_ref->sem, 1);
		dummy_page.state = PAGE_STATE_INACTIVE;
	}

	vm_page_set_state(page, PAGE_STATE_ACTIVE);
	
	vm_cache_release_ref(cache_ref);

	return err;
}

vm_region *vm_virtual_map_lookup(vm_virtual_map *map, addr address)
{
	vm_region *region;

	// check the region_list region first
	region = map->region_hint;
	if(region && region->base <= address && (region->base + region->size) > address)
		return region;

	for(region = map->region_list; region != NULL; region = region->next) {
		if(region->base <= address && (region->base + region->size) > address)
			break;
	}

	if(region)
		map->region_hint = region;
	return region;
}


