/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/vm_page.h>
#include <kernel/vm_cache.h>
#include <kernel/vm_store_anonymous_noswap.h>
#include <kernel/vm_store_device.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/sem.h>
#include <kernel/lock.h>
#include <kernel/khash.h>
#include <sys/errors.h>

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

#define REGION_HASH_TABLE_SIZE 1024
static region_id next_region_id;
static void *region_table;
static sem_id region_hash_sem;

#define ASPACE_HASH_TABLE_SIZE 1024
static aspace_id next_aspace_id;
static void *aspace_table;
static sem_id aspace_hash_sem;

static int max_commit;
static spinlock_t max_commit_lock;

// function declarations
static vm_region *_vm_create_region_struct(vm_address_space *aspace, const char *name, int wiring, int lock);
static int map_backing_store(vm_address_space *aspace, vm_store *store, void **vaddr, 
	off_t offset, addr size, int addr_type, int wiring, int lock, vm_region **_region, const char *region_name);
static int vm_soft_fault(addr address, bool is_write, bool is_user);
static vm_region *vm_virtual_map_lookup(vm_virtual_map *map, addr address);
static int vm_region_acquire_ref(vm_region *region);
static void vm_region_release_ref(vm_region *region);
static void vm_region_release_ref2(vm_region *region);

static int region_compare(void *_r, void *key)
{
	vm_region *r = _r;
	region_id *id = key;

	if(r->id == *id)
		return 0;
	else
		return -1;
}

static unsigned int region_hash(void *_r, void *key, int range)
{
	vm_region *r = _r;
	region_id *id = key;
	
	if(r != NULL)
		return (r->id % range);
	else
		return (*id % range);
}

static int aspace_compare(void *_a, void *key)
{
	vm_address_space *aspace = _a;
	aspace_id *id = key;

	if(aspace->id == *id)
		return 0;
	else
		return -1;
}

static unsigned int aspace_hash(void *_a, void *key, int range)
{
	vm_address_space *aspace = _a;
	aspace_id *id = key;
	
	if(aspace != NULL)
		return (aspace->id % range);
	else
		return (*id % range);
}

vm_address_space *vm_get_aspace_from_id(aspace_id aid)
{
	vm_address_space *aspace;
	
	sem_acquire(aspace_hash_sem, READ_COUNT);
	aspace = hash_lookup(aspace_table, &aid);
	sem_release(aspace_hash_sem, READ_COUNT);
	
	return aspace;
}

region_id vm_find_region_by_name(aspace_id aid, const char *name)
{
	vm_region *region = NULL;
	vm_address_space *aspace;
	region_id id = ERR_NOT_FOUND;
	
	aspace = vm_get_aspace_from_id(aid);
	if(aspace == NULL)
		return ERR_VM_INVALID_ASPACE;

	sem_acquire(aspace->virtual_map.sem, READ_COUNT);

	region = aspace->virtual_map.region_list;
	while(region != NULL) {
		if(strcmp(region->name, name) == 0) {
			id = region->id;
			break;
		}
		region = region->aspace_next;
	}

	sem_release(aspace->virtual_map.sem, READ_COUNT);
	return id;
}

static vm_region *_vm_create_region_struct(vm_address_space *aspace, const char *name, int wiring, int lock)
{
	vm_region *region = NULL;

	region = (vm_region *)kmalloc(sizeof(vm_region));
	if(region == NULL)
		return NULL;
	region->name = (char *)kmalloc(strlen(name) + 1);
	if(region->name == NULL) {
		kfree(region);
		return NULL;
	}
	strcpy(region->name, name);
	region->id = atomic_add(&next_region_id, 1);
	region->base = 0;
	region->size = 0;
	region->lock = lock;
	region->wiring = wiring;
	region->ref_count = 1;

	region->cache_ref = NULL;
	region->cache_offset = 0;

	region->aspace = aspace;
	region->aspace_next = NULL;
	region->map = &aspace->virtual_map;
	region->cache_next = region->cache_prev = NULL;
	region->hash_next = NULL;

	return region;
}

// must be called with this address space's virtual_map.sem held
static int find_and_insert_region_slot(vm_virtual_map *map, addr start, addr size, addr end, int addr_type, vm_region *region)
{
	vm_region *last_r = NULL;
	vm_region *next_r;
	bool foundspot = false;

	dprintf("find_and_insert_region_slot: map 0x%x, start 0x%x, size %d, end 0x%x, addr_type %d, region 0x%x\n",
		map, start, size, end, addr_type, region);
	dprintf("map->base 0x%x, map->size 0x%x\n", map->base, map->size);

	// do some sanity checking
	if(start < map->base || size == 0 || (end - 1) > (map->base + (map->size - 1)) || start + size > end)
		return ERR_VM_BAD_ADDRESS;

	// walk up to the spot where we should start searching
	next_r = map->region_list;
	while(next_r) {
		if(next_r->base >= start + size) {
			// we have a winner
			break;
		}
		last_r = next_r;
		next_r = next_r->aspace_next;
	}

	dprintf("last_r 0x%x, next_r 0x%x\n", last_r, next_r);
	if(last_r) dprintf("last_r->base 0x%x, last_r->size 0x%x\n", last_r->base, last_r->size);
	if(next_r) dprintf("next_r->base 0x%x, next_r->size 0x%x\n", next_r->base, next_r->size);

	switch(addr_type) {
		case REGION_ADDR_ANY_ADDRESS:
			// find a hole big enough for a new region
			if(!last_r) {
				// see if we can build it at the beginning of the virtual map
				if(!next_r || (next_r->base >= map->base + size)) {
					foundspot = true;
					region->base = map->base;
					break;
				}
				last_r = next_r;
				next_r = next_r->aspace_next;
			}
			// keep walking
			while(next_r) {
				if(next_r->base >= last_r->base + last_r->size + size) {
					// we found a spot
					foundspot = true;
					region->base = last_r->base + last_r->size;
					break;
				}
				last_r = next_r;
				next_r = next_r->aspace_next;
			}
			if((map->base + (map->size - 1)) >= (last_r->base + last_r->size + (size - 1))) {
				// found a spot
				foundspot = true;
				region->base = last_r->base + last_r->size;
				break;
			}	
			break;
		case REGION_ADDR_EXACT_ADDRESS:
			// see if we can create it exactly here
			if(!last_r) {
				if(!next_r || (next_r->base >= start + size)) {
					foundspot = true;
					region->base = start;
					break;
				}
			} else {
				if(next_r) {
					if(last_r->base + last_r->size <= start && next_r->base >= start + size) {
						foundspot = true;
						region->base = start;
						break;
					}
				} else {
					if((last_r->base + (last_r->size - 1)) <= start - 1) {
						foundspot = true;
						region->base = start;
					}
				}
			}
			break;
		default:
			return ERR_INVALID_ARGS;
	}

	if(foundspot) {
		region->size = size;
		if(last_r) {
			region->aspace_next = last_r->aspace_next;
			last_r->aspace_next = region;
		} else {
			region->aspace_next = map->region_list;
			map->region_list = region;
		}
		return NO_ERROR;
	} else {
		return ERR_VM_NO_REGION_SLOT;
	}
}

// a ref to the cache holding this store must be held before entering here
static int map_backing_store(vm_address_space *aspace, vm_store *store, void **vaddr, 
	off_t offset, addr size, int addr_type, int wiring, int lock, vm_region **_region, const char *region_name)
{
	vm_cache *cache;
	vm_cache_ref *cache_ref;
	vm_region *region;
	int err;

//	dprintf("map_backing_store: aspace 0x%x, store 0x%x, *vaddr 0x%x, offset 0x%x 0x%x, size %d, addr_type %d, wiring %d, lock %d, _region 0x%x, region_name '%s'\n",
//		aspace, store, *vaddr, offset, size, addr_type, wiring, lock, _region, region_name);

	region = _vm_create_region_struct(aspace, region_name, wiring, lock);
	if(!region)
		return ERR_NO_MEMORY;

	cache = store->cache;
	cache_ref = cache->ref;

	mutex_lock(&cache_ref->lock);
	if(store->committed_size < offset + size) {
		// try to commit more memory
		off_t old_store_commitment = store->committed_size;
		off_t commitment = (store->ops->commit)(store, offset + size);
		if(commitment < offset + size) {
			if(cache->temporary) {
				int state = int_disable_interrupts();
				acquire_spinlock(&max_commit_lock);

				if(max_commit - old_store_commitment + commitment < offset + size) {
					release_spinlock(&max_commit_lock);
					int_restore_interrupts(state);
					mutex_unlock(&cache_ref->lock);
					err = ERR_VM_WOULD_OVERCOMMIT;
					goto err;
				}

//				dprintf("map_backing_store: adding %d to max_commit\n", 
//					(commitment - old_store_commitment) - (offset + size - cache->committed_size));

				max_commit += (commitment - old_store_commitment) - (offset + size - cache->virtual_size);
				cache->virtual_size = offset + size;
				release_spinlock(&max_commit_lock);
				int_restore_interrupts(state);
			} else {
				mutex_unlock(&cache_ref->lock);
				err = ERR_NO_MEMORY;
				goto err;
			}
		}
	}

	mutex_unlock(&cache_ref->lock);

	vm_cache_acquire_ref(cache_ref);
	
	sem_acquire(aspace->virtual_map.sem, WRITE_COUNT);

	{
		addr search_addr, search_end;

		if(addr_type == REGION_ADDR_EXACT_ADDRESS) {
			search_addr = (addr)*vaddr;
			search_end = (addr)*vaddr + size;
		} else if(addr_type == REGION_ADDR_ANY_ADDRESS) {
			search_addr = aspace->virtual_map.base;
			search_end = aspace->virtual_map.base + (aspace->virtual_map.size - 1);
		} else {
			err = ERR_INVALID_ARGS;
			goto err2;
		}
		
		err = find_and_insert_region_slot(&aspace->virtual_map, search_addr, size, search_end, addr_type, region);
		if(err < 0)
			goto err2;
		*vaddr = (addr *)region->base;
	}

	// attach the cache to the region
	region->cache_ref = cache_ref;
	region->cache_offset = offset;	
	// point the cache back to the region
	vm_cache_insert_region(cache_ref, region);

	// insert the region in the global region hash table
	sem_acquire(region_hash_sem, WRITE_COUNT);
	hash_insert(region_table, region);
	sem_release(region_hash_sem, WRITE_COUNT);
	
	sem_release(aspace->virtual_map.sem, WRITE_COUNT);

	*_region = region;

	return NO_ERROR;

err2:
	sem_release(aspace->virtual_map.sem, WRITE_COUNT);
err1:
	// releasing the cache ref twice should put us back to the proper cache count
	vm_cache_release_ref(cache_ref);
err:
	kfree(region->name);
	kfree(region);
	return err;
}

region_id user_vm_create_anonymous_region(aspace_id aid, char *uname, void **uaddress, int addr_type,
	addr size, int wiring, int lock)
{
	char name[SYS_MAX_OS_NAME_LEN];
	void *address;
	int rc, rc2;
	
	if((addr)uname >= KERNEL_BASE && (addr)uname <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(name, uname, SYS_MAX_OS_NAME_LEN-1);
	if(rc < 0)
		return rc;
	name[SYS_MAX_OS_NAME_LEN-1] = 0;

	rc = user_memcpy(&address, uaddress, sizeof(address));
	if(rc < 0)
		return rc;

	rc = vm_create_anonymous_region(aid, name, &address, addr_type, size, wiring, lock);
	if(rc < 0)
		return rc;

	rc2 = user_memcpy(uaddress, &address, sizeof(address));
	if(rc2 < 0)
		return rc2;

	return rc;
}

region_id vm_create_anonymous_region(aspace_id aid, char *name, void **address, int addr_type,
	addr size, int wiring, int lock)
{
	int err;
	vm_region *region;
	vm_cache *cache;
	vm_store *store;
	vm_address_space *aspace;
	vm_cache_ref *cache_ref;

	aspace = vm_get_aspace_from_id(aid);
	if(aspace == NULL)
		return ERR_VM_INVALID_ASPACE;

	size = PAGE_ALIGN(size);

	// create an anonymous store object
	store = vm_store_create_anonymous_noswap();
	if(store == NULL)
		panic("vm_create_anonymous_region: vm_create_store_anonymous_noswap returned NULL");
	cache = vm_cache_create(store);
	if(cache == NULL)
		panic("vm_create_anonymous_region: vm_cache_create returned NULL");
	cache_ref = vm_cache_ref_create(cache);
	if(cache_ref == NULL)
		panic("vm_create_anonymous_region: vm_cache_ref_create returned NULL");
	cache->temporary = 1;

	vm_cache_acquire_ref(cache_ref);
	err = map_backing_store(aspace, store, address, 0, size, addr_type, wiring, lock, &region, name);
	vm_cache_release_ref(cache_ref);
	if(err < 0)
		return err;

	cache_ref = store->cache->ref;
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

			mutex_lock(&cache_ref->lock);
			(*aspace->translation_map.ops->lock)(&aspace->translation_map);
			for(va = region->base; va < region->base + region->size; va += PAGE_SIZE, offset += PAGE_SIZE) {
				err = (*aspace->translation_map.ops->query)(&aspace->translation_map,
					va, &pa, &flags);
				if(err < 0) {
//					dprintf("vm_create_anonymous_region: error looking up mapping for va 0x%x\n", va);
					continue;
				}
				page = vm_lookup_page(pa / PAGE_SIZE);
				if(page == NULL) {
//					dprintf("vm_create_anonymous_region: error looking up vm_page structure for pa 0x%x\n", pa);
					continue;
				}
				vm_page_set_state(page, PAGE_STATE_WIRED);
				vm_cache_insert_page(cache_ref, page, offset);
			}
			(*aspace->translation_map.ops->unlock)(&aspace->translation_map);
			mutex_unlock(&cache_ref->lock);
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

			mutex_lock(&cache_ref->lock);
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
			mutex_unlock(&cache_ref->lock);

			break;
		}
		default:
			;		
	}
	if(region)
		return region->id;
	else
		return ERR_NO_MEMORY;
}

region_id vm_map_physical_memory(aspace_id aid, char *name, void **address, int addr_type,
	addr size, int lock, addr phys_addr)
{
	vm_region *region;
	vm_cache *cache;
	vm_cache_ref *cache_ref;
	vm_store *store;
	addr map_offset;
	int err;
	
	vm_address_space *aspace = vm_get_aspace_from_id(aid);
	if(aspace == NULL)
		return ERR_VM_INVALID_ASPACE;

	// if the physical address is somewhat inside a page, 
	// move the actual region down to align on a page boundary
	map_offset = phys_addr % PAGE_SIZE;
	size += map_offset;
	phys_addr -= map_offset;

	size = PAGE_ALIGN(size);

	// create an device store object
	store = vm_store_create_device(phys_addr);
	if(store == NULL)
		panic("vm_map_physical_memory: vm_store_create_device returned NULL");
	cache = vm_cache_create(store);
	if(cache == NULL)
		panic("vm_map_physical_memory: vm_cache_create returned NULL");
	cache_ref = vm_cache_ref_create(cache);
	if(cache_ref == NULL)
		panic("vm_map_physical_memory: vm_cache_ref_create returned NULL");

	vm_cache_acquire_ref(cache_ref);
	err = map_backing_store(aspace, store, address, 0, size, addr_type, 0, lock, &region, name);
	vm_cache_release_ref(cache_ref);
	if(err < 0)
		return err;

	// modify the pointer returned to be offset back into the new region
	// the same way the physical address in was offset
	(*address) += map_offset;
	return region->id;
}

region_id user_vm_clone_region(aspace_id aid, char *uname, void **uaddress, int addr_type,
	region_id source_region, int lock)
{
	char name[SYS_MAX_OS_NAME_LEN];
	void *address;
	int rc, rc2;
	
	if((addr)uname >= KERNEL_BASE && (addr)uname <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(name, uname, SYS_MAX_OS_NAME_LEN-1);
	if(rc < 0)
		return rc;
	name[SYS_MAX_OS_NAME_LEN-1] = 0;

	rc = user_memcpy(&address, uaddress, sizeof(address));
	if(rc < 0)
		return rc;

	rc = vm_clone_region(aid, name, &address, addr_type, source_region, lock);
	if(rc < 0)
		return rc;

	rc2 = user_memcpy(uaddress, &address, sizeof(address));
	if(rc2 < 0)
		return rc2;

	return rc;
}
	
region_id vm_clone_region(aspace_id aid, char *name, void **address, int addr_type,
	region_id source_region, int lock)
{
	vm_region *new_region;
	vm_region *src_region;
	int err;
	
	vm_address_space *aspace = vm_get_aspace_from_id(aid);
	if(aspace == NULL)
		return ERR_VM_INVALID_ASPACE;

	// look up the source region
	sem_acquire(region_hash_sem, READ_COUNT);
	src_region = hash_lookup(region_table, &source_region);
	if(src_region != NULL) {
		if(vm_region_acquire_ref(src_region) < 0) {
			// it's ref has already been decremented, so it's not reliable
			src_region = NULL;
		}
	}
	sem_release(region_hash_sem, READ_COUNT);
	
	if(src_region == NULL)
		return ERR_VM_INVALID_REGION;	

	if(vm_cache_acquire_ref(src_region->cache_ref) <= 0)
		return ERR_VM_GENERAL;
	err = map_backing_store(aspace, src_region->cache_ref->cache->store, address, src_region->cache_offset, src_region->size, 
		addr_type, src_region->wiring, lock, &new_region, name);
	vm_cache_release_ref(src_region->cache_ref);

	// release the ref on the old region
	vm_region_release_ref(src_region);

	if(err < 0)
		return err;
	else
		return new_region->id;
}

int vm_delete_region(aspace_id aid, region_id rid)
{
	vm_region *temp, *last = NULL;
	vm_region *region;
	vm_address_space *aspace;

	aspace = vm_get_aspace_from_id(aid);
	if(aspace == NULL)
		return ERR_VM_INVALID_ASPACE;

	dprintf("vm_delete_region: aspace 0x%x, region 0x%x\n", aspace, rid);

	// find the region structure
	sem_acquire(region_hash_sem, READ_COUNT);
	region = hash_lookup(region_table, &rid);
	if(region != NULL) {
		// acquire the ref once, to make sure no old else deletes this
		// region before we can do it
		if(vm_region_acquire_ref(region) < 0) {
			region = NULL;
		}
	}
	sem_release(region_hash_sem, READ_COUNT);
	if(region == NULL)
		return ERR_VM_INVALID_REGION;

	// release the ref twice, insuring no one else got this far
	vm_region_release_ref2(region);

	return 0;
}

static int vm_region_acquire_ref(vm_region *region)
{
	if(region == NULL)
		panic("vm_region_acquire_ref: passed NULL\n");
	if(atomic_add(&region->ref_count, 1) == 0)
		return -1;
	return 0;
}

static void vm_remove_region_struct(vm_region *region)
{
	// time to die
	vm_region *temp, *last = NULL;
	vm_address_space *aspace = region->aspace;
	
	// remove the region from the aspace's virtual map
	sem_acquire(aspace->virtual_map.sem, WRITE_COUNT);
	temp = aspace->virtual_map.region_list;
	while(temp != NULL) {
		if(region == temp) {
			if(last != NULL) {
				last->aspace_next = temp->aspace_next;
			} else {
				aspace->virtual_map.region_list = temp->aspace_next;
			}
			break;
		}
		last = temp;
		temp = temp->aspace_next;
	}
	if(region == aspace->virtual_map.region_hint)
		aspace->virtual_map.region_hint = NULL;
	sem_release(aspace->virtual_map.sem, WRITE_COUNT);

	if(temp == NULL)
		panic("vm_region_release_ref: region not found in aspace's region_list\n");

	// remove the region from the global hash table
	sem_acquire(region_hash_sem, WRITE_COUNT);
	hash_remove(region_table, region);
	sem_release(region_hash_sem, WRITE_COUNT);

	vm_cache_remove_region(region->cache_ref, region);
	vm_cache_release_ref(region->cache_ref);

	(*aspace->translation_map.ops->lock)(&aspace->translation_map);
	(*aspace->translation_map.ops->unmap)(&aspace->translation_map, region->base,
		region->base + (region->size - 1));
	(*aspace->translation_map.ops->unlock)(&aspace->translation_map);

	if(region->name)
		kfree(region->name);
	kfree(region);
}

static void vm_region_release_ref(vm_region *region)
{
	if(region == NULL)
		panic("vm_region_release_ref: passed NULL\n");
	if(atomic_add(&region->ref_count, -1) == 1) {
		vm_remove_region_struct(region);
	}
}

static void vm_region_release_ref2(vm_region *region)
{
	if(region == NULL)
		panic("vm_region_release_ref2: passed NULL\n");
	if(atomic_add(&region->ref_count, -2) <= 2) {
		vm_remove_region_struct(region);
	}
}

int user_vm_get_region_info(region_id id, vm_region_info *uinfo)
{
	vm_region_info info;
	int rc, rc2;

	if((addr)uinfo >= KERNEL_BASE && (addr)uinfo <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = vm_get_region_info(id, &info);
	if(rc < 0)
		return rc;

	rc2 = user_memcpy(uinfo, &info, sizeof(info));
	if(rc2 < 0)
		return rc2;

	return rc;
}

int vm_get_region_info(region_id id, vm_region_info *info)
{
	vm_region *region;

	if(info == NULL)
		return ERR_INVALID_ARGS;

	// remove the region from the global hash table
	sem_acquire(region_hash_sem, READ_COUNT);
	region = hash_lookup(region_table, &id);
	if(region != NULL) {
		if(vm_region_acquire_ref(region) < 0)
			region = NULL;
	}
	sem_release(region_hash_sem, READ_COUNT);
	if(region == NULL)
		return ERR_VM_INVALID_REGION;	

	info->id = region->id;
	info->base = region->base;
	info->size = region->size;
	info->lock = region->lock;
	info->wiring = region->wiring;
	strncpy(info->name, region->name, SYS_MAX_OS_NAME_LEN-1);
	info->name[SYS_MAX_OS_NAME_LEN-1] = 0;
	
	vm_region_release_ref(region);
	
	return 0;
}

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

static void dump_cache_ref(int argc, char **argv)
{
	addr address;
	vm_region *region;
	vm_cache_ref *cache_ref;

	if(argc < 2) {
		dprintf("cache_ref: not enough arguments\n");
		return;
	}
	if(strlen(argv[1]) < 2 || argv[1][0] != '0' || argv[1][1] != 'x') {
		dprintf("cache_ref: invalid argument, pass address\n");
		return;
	}	

	address = atoul(argv[1]);
	cache_ref = (vm_cache_ref *)address;
	
	dprintf("cache_ref at 0x%x:\n", cache_ref);
	dprintf("cache: 0x%x\n", cache_ref->cache);
	dprintf("lock.count: %d\n", cache_ref->lock.count);
	dprintf("lock.sem: 0x%x\n", cache_ref->lock.sem);
	dprintf("region_list:\n");
	for(region = cache_ref->region_list; region != NULL; region = region->cache_next) {
		dprintf(" region 0x%x: ", region->id);
		dprintf("base_addr = 0x%x ", region->base);
		dprintf("size = 0x%x ", region->size);
		dprintf("name = '%s' ", region->name);
		dprintf("lock = 0x%x\n", region->lock);
	}
	dprintf("ref_count: %d\n", cache_ref->ref_count);
}

static const char *page_state_to_text(int state)
{
	switch(state) {
		case PAGE_STATE_ACTIVE:
			return "active";
		case PAGE_STATE_INACTIVE:
			return "inactive";
		case PAGE_STATE_BUSY:
			return "busy";
		case PAGE_STATE_MODIFIED:
			return "modified";
		case PAGE_STATE_FREE:
			return "free";
		case PAGE_STATE_CLEAR:
			return "clear";
		case PAGE_STATE_WIRED:
			return "wired";
		case PAGE_STATE_UNUSED:
			return "unused";
		default:
			return "unknown";
	}
}

static void dump_cache(int argc, char **argv)
{
	addr address;
	vm_cache *cache;
	vm_page *page;

	if(argc < 2) {
		dprintf("cache: not enough arguments\n");
		return;
	}
	if(strlen(argv[1]) < 2 || argv[1][0] != '0' || argv[1][1] != 'x') {
		dprintf("cache: invalid argument, pass address\n");
		return;
	}	

	address = atoul(argv[1]);
	cache = (vm_cache *)address;

	dprintf("cache at 0x%x:\n", cache);
	dprintf("cache_ref: 0x%x\n", cache->ref);
	dprintf("store: 0x%x\n", cache->store);
	// XXX 64-bit
	dprintf("virtual_size: 0x%x 0x%x\n", cache->virtual_size);
	dprintf("temporary: %d\n", cache->temporary);
	dprintf("page_list:\n");
	for(page = cache->page_list; page != NULL; page = page->cache_next) {
		// XXX offset is 64-bit
		dprintf(" 0x%x ppn 0x%x offset 0x%x 0x%x type %d state %d (%s) ref_count %d\n",
			page, page->ppn, page->offset, page->type, page->state, page_state_to_text(page->state), page->ref_count);
	}
}

static void _dump_region(vm_region *region)
{
	dprintf("dump of region at 0x%x:\n", region);
	dprintf("name: '%s'\n", region->name);
	dprintf("id: 0x%x\n", region->id);
	dprintf("base: 0x%x\n", region->base);
	dprintf("size: 0x%x\n", region->size);
	dprintf("lock: 0x%x\n", region->lock);
	dprintf("wiring: 0x%x\n", region->wiring);
	dprintf("ref_count: %d\n", region->ref_count);
	dprintf("cache_ref: 0x%x\n", region->cache_ref);
	// XXX 64-bit
	dprintf("cache_offset: 0x%x 0x%x\n", region->cache_offset);
	dprintf("cache_next: 0x%x\n", region->cache_next);
	dprintf("cache_prev: 0x%x\n", region->cache_prev);
}

static void dump_region(int argc, char **argv)
{
	int i;
	vm_region *region;

	if(argc < 2) {
		dprintf("region: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		unsigned long num = atoul(argv[1]);
		region_id id = num;

		region = hash_lookup(region_table, &id);
		if(region == NULL) {
			dprintf("invalid region id\n");
		} else {
			_dump_region(region);
		}
		return;
	} else {	
		// walk through the region list, looking for the arguments as a name
		struct hash_iterator iter;
		
		hash_open(region_table, &iter);
		while((region = hash_next(region_table, &iter)) != NULL) {
			if(region->name != NULL && strcmp(argv[1], region->name) == 0) {
				_dump_region(region);
			}
		}
	}
}

static void dump_region_list(int argc, char **argv)
{
	vm_region *region;
	struct hash_iterator iter;

	dprintf("addr\tid\t%32s\tbase\t\tsize\tlock\twiring\n", "name");

	hash_open(region_table, &iter);
	while((region = hash_next(region_table, &iter)) != NULL) {
		dprintf("0x%x\t0x%x\t%32s\t0x%x\t\t0x%x\t%d\t%d\n",
			region, region->id, region->name, region->base, region->size, region->lock, region->wiring);
	}
	hash_close(region_table, &iter, false);
}

static void _dump_aspace(vm_address_space *aspace)
{
	vm_region *region;

	dprintf("dump of address space at 0x%x:\n", aspace);
	dprintf("name: '%s'\n", aspace->name);
	dprintf("id: 0x%x\n", aspace->id);
	dprintf("translation_map: 0x%x\n", &aspace->translation_map);
	dprintf("virtual_map.base: 0x%x\n", aspace->virtual_map.base);
	dprintf("virtual_map.size: 0x%x\n", aspace->virtual_map.size);
	dprintf("virtual_map.change_count: 0x%x\n", aspace->virtual_map.change_count);
	dprintf("virtual_map.sem: 0x%x\n", aspace->virtual_map.sem);
	dprintf("virtual_map.region_hint: 0x%x\n", aspace->virtual_map.region_hint);
	dprintf("virtual_map.region_list:\n");
	for(region = aspace->virtual_map.region_list; region != NULL; region = region->aspace_next) {
		dprintf(" region 0x%x: ", region->id);
		dprintf("base_addr = 0x%x ", region->base);
		dprintf("size = 0x%x ", region->size);
		dprintf("name = '%s' ", region->name);
		dprintf("lock = 0x%x\n", region->lock);
	}
}

static void dump_aspace(int argc, char **argv)
{
	int i;
	vm_address_space *aspace;

	if(argc < 2) {
		dprintf("aspace: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		unsigned long num = atoul(argv[1]);
		aspace_id id = num;

		aspace = hash_lookup(aspace_table, &id);
		if(aspace == NULL) {
			dprintf("invalid aspace id\n");
		} else {
			_dump_aspace(aspace);
		}
		return;
	} else {	
		// walk through the aspace list, looking for the arguments as a name
		struct hash_iterator iter;
		
		hash_open(aspace_table, &iter);
		while((aspace = hash_next(aspace_table, &iter)) != NULL) {
			if(aspace->name != NULL && strcmp(argv[1], aspace->name) == 0) {
				_dump_aspace(aspace);
			}
		}
	}
}

static void dump_aspace_list(int argc, char **argv)
{
	vm_address_space *as;
	struct hash_iterator iter;

	dprintf("addr\tid\t%32s\tbase\t\tsize\n", "name");

	hash_open(aspace_table, &iter);
	while((as = hash_next(aspace_table, &iter)) != NULL) {
		dprintf("0x%x\t0x%x\t%32s\t0x%x\t\t0x%x\n",
			as, as->id, as->name, as->virtual_map.base, as->virtual_map.size);
	}
	hash_close(aspace_table, &iter, false);
}

vm_address_space *vm_get_kernel_aspace()
{
	return kernel_aspace;
}

aspace_id vm_get_kernel_aspace_id()
{
	return kernel_aspace->id;
}

vm_address_space *vm_get_current_user_aspace()
{
	return vm_get_aspace_from_id(vm_get_current_user_aspace_id());
}

aspace_id vm_get_current_user_aspace_id()
{
	return thread_get_current_thread()->proc->aspace_id;
}

aspace_id vm_create_aspace(const char *name, addr base, addr size, bool kernel)
{
	vm_address_space *aspace;
	int err;
	
	aspace = (vm_address_space *)kmalloc(sizeof(vm_address_space));
	if(aspace == NULL)
		return ERR_NO_MEMORY;

	aspace->name = (char *)kmalloc(strlen(name) + 1);
	if(aspace->name == NULL ) {
		kfree(aspace);
		return ERR_NO_MEMORY;
	}
	strcpy(aspace->name, name);

	aspace->id = next_aspace_id++;

	// initialize the corresponding translation map
	err = vm_translation_map_create(&aspace->translation_map, kernel);
	if(err < 0) {
		kfree(aspace->name);
		kfree(aspace);
		return err;
	}

	// initialize the virtual map
	aspace->virtual_map.base = base;
	aspace->virtual_map.size = size;
	aspace->virtual_map.region_list = NULL;
	aspace->virtual_map.region_hint = NULL;
	aspace->virtual_map.change_count = 0;
	aspace->virtual_map.sem = sem_create(WRITE_COUNT, "aspacelock");
	aspace->virtual_map.aspace = aspace;

	// add the aspace to the global hash table
	sem_acquire(aspace_hash_sem, WRITE_COUNT);
	hash_insert(aspace_table, aspace);
	sem_release(aspace_hash_sem, WRITE_COUNT);

	return aspace->id;
}

int vm_delete_aspace(aspace_id aid)
{
	vm_region *region;
	vm_address_space *aspace;

	aspace = vm_get_aspace_from_id(aid);
	if(aspace == NULL)
		return ERR_VM_INVALID_ASPACE;

	dprintf("vm_delete_aspace: called on aspace 0x%x\n", aid);

	// delete all of the regions
	while(aspace->virtual_map.region_list) {
		vm_delete_region(aid, aspace->virtual_map.region_list->id);
	}

	(*aspace->translation_map.ops->destroy)(&aspace->translation_map);

	// remove the aspace from the global hash table
	sem_acquire(aspace_hash_sem, WRITE_COUNT);
	hash_remove(aspace_table, aspace);
	sem_release(aspace_hash_sem, WRITE_COUNT);

	kfree(aspace->name);
	sem_delete(aspace->virtual_map.sem);
	kfree(aspace);
	return 0;
}

int vm_thread_dump_max_commit()
{
	int oldmax = -1;

	for(;;) {
		thread_snooze(1000000);
		if(oldmax != max_commit)
			dprintf("max_commit 0x%x\n", max_commit);
		oldmax = max_commit;
	}
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
	region_hash_sem = -1;
	next_aspace_id = 0;
	aspace_hash_sem = -1;
	max_commit = 0; // will be increased in vm_page_init
	max_commit_lock = 0;

	// map in the new heap and initialize it
	heap_base = vm_alloc_from_ka_struct(ka, HEAP_SIZE, LOCK_KERNEL|LOCK_RW);
	dprintf("heap at 0x%x\n", heap_base);
	heap_init(heap_base, HEAP_SIZE);

	// initialize the free page list and physical page mapper
	vm_page_init(ka);

	// create the region and address space hash tables
	{
		vm_address_space *aspace;
		aspace_table = hash_init(ASPACE_HASH_TABLE_SIZE, (addr)&aspace->hash_next - (addr)aspace,
			&aspace_compare, &aspace_hash);
		if(aspace_table == NULL)
			panic("vm_init: error creating aspace hash table\n");
	}
	{
		vm_region *region;
		region_table = hash_init(REGION_HASH_TABLE_SIZE, (addr)&region->hash_next - (addr)region,
			&region_compare, &region_hash);
		if(region_table == NULL)
			panic("vm_init: error creating aspace hash table\n");
	}


	// create the initial kernel address space
	{
		aspace_id aid;
		aid = vm_create_aspace("kernel_land", KERNEL_BASE, KERNEL_SIZE, true);
		if(aid < 0)
			panic("vm_init: error creating kernel address space!\n");
		kernel_aspace = vm_get_aspace_from_id(aid);
	}

	// do any further initialization that the architecture dependant layers may need now
	vm_translation_map_module_init2(ka);
	arch_vm_init2(ka);
	vm_page_init2(ka);

	// allocate regions to represent stuff that already exists
	null_addr = (void *)ROUNDOWN(heap_base, PAGE_SIZE);
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "kernel_heap", &null_addr, REGION_ADDR_EXACT_ADDRESS,
		HEAP_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	null_addr = (void *)ROUNDOWN(ka->kernel_seg0_addr.start, PAGE_SIZE);
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "kernel_seg0", &null_addr, REGION_ADDR_EXACT_ADDRESS,
		PAGE_ALIGN(ka->kernel_seg0_addr.size), REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	if(ka->kernel_seg1_addr.size > 0) {
		null_addr = (void *)ROUNDOWN(ka->kernel_seg1_addr.start, PAGE_SIZE);
		vm_create_anonymous_region(vm_get_kernel_aspace_id(), "kernel_seg1", &null_addr, REGION_ADDR_EXACT_ADDRESS,
			PAGE_ALIGN(ka->kernel_seg1_addr.size), REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
	}
	for(i=0; i < ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d_kstack", i);
		null_addr = (void *)ka->cpu_kstack[i].start;
		vm_create_anonymous_region(vm_get_kernel_aspace_id(), temp, &null_addr, REGION_ADDR_EXACT_ADDRESS,
			ka->cpu_kstack[i].size, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
	}
	{
		void *null;
		vm_map_physical_memory(vm_get_kernel_aspace_id(), "bootdir", &null, REGION_ADDR_ANY_ADDRESS,
			ka->bootdir_addr.size, LOCK_RO|LOCK_KERNEL, ka->bootdir_addr.start);
	}

	// add some debugger commands
	dbg_add_command(&dump_region_list, "regions", "Dump a list of all regions");
	dbg_add_command(&dump_region, "region", "Dump info about a particular region");
	dbg_add_command(&dump_aspace_list, "aspaces", "Dump a list of all address spaces");
	dbg_add_command(&dump_aspace, "aspace", "Dump info about a particular address space");
	dbg_add_command(&dump_cache_ref, "cache_ref", "Dump cache_ref data structure");
	dbg_add_command(&dump_cache, "cache", "Dump cache_ref data structure");
//	dbg_add_command(&display_mem, "dl", "dump memory long words (64-bit)");
	dbg_add_command(&display_mem, "dw", "dump memory words (32-bit)");
	dbg_add_command(&display_mem, "ds", "dump memory shorts (16-bit)");
	dbg_add_command(&display_mem, "db", "dump memory bytes (8-bit)");

	dprintf("vm_init: exit\n");

	return err;
}

int vm_init_postsem(kernel_args *ka)
{
	vm_region *region;
	
	// fill in all of the semaphores that were not allocated before
	// since we're still single threaded and only the kernel address space exists,
	// it isn't that hard to find all of the ones we need to create
	kernel_aspace->virtual_map.sem = sem_create(WRITE_COUNT, "kernel_aspacelock");
	kernel_aspace->translation_map.lock.sem = sem_create(1, "recursive_lock");
	
	for(region = kernel_aspace->virtual_map.region_list; region; region = region->aspace_next) {
		if(region->cache_ref->lock.sem < 0) {
			region->cache_ref->lock.sem = sem_create(1, "cache_ref_mutex");
		}
	}

	region_hash_sem = sem_create(WRITE_COUNT, "region_hash_sem");
	aspace_hash_sem = sem_create(WRITE_COUNT, "aspace_hash_sem");

	return heap_init_postsem(ka);
}

int vm_init_postthread(kernel_args *ka)
{
	vm_page_init_postthread(ka);

	{
		thread_id tid = thread_create_kernel_thread("max_commit_thread", &vm_thread_dump_max_commit, 30);
		thread_resume_thread(tid);
	}	

	return 0;
}

int vm_page_fault(addr address, addr fault_address, bool is_write, bool is_user, addr *newip)
{
	int err;

	dprintf("vm_page_fault: page fault at 0x%x, ip 0x%x\n", address, fault_address);

	*newip = 0;

	err = vm_soft_fault(address, is_write, is_user);
	if(err < 0) {
		if(!is_user) {
			struct thread *t = thread_get_current_thread();
			if(t->fault_handler != 0) {
				// this will cause the arch dependant page fault handler to 
				// modify the IP on the interrupt frame or whatever to return
				// to this address
				*newip = t->fault_handler;
			} else {
				// unhandled page fault in the kernel
				panic("vm_page_fault: unhandled page fault in kernel space at 0x%x, ip 0x%x\n",
					address, fault_address);
			}
		} else {
			dprintf("vm_page_fault: killing process 0x%x\n", thread_get_current_thread()->proc->id);
			proc_kill_proc(thread_get_current_thread()->proc->id);
		}
	}

	return INT_NO_RESCHEDULE;
}

static int vm_soft_fault(addr address, bool is_write, bool is_user)
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

	dprintf("vm_soft_fault: thid 0x%x address 0x%x, is_write %d, is_user %d\n",
		thread_get_current_thread_id(), address, is_write, is_user);
	
	address = ROUNDOWN(address, PAGE_SIZE);
	
	if(address >= KERNEL_BASE && address <= KERNEL_TOP) {
		aspace = vm_get_kernel_aspace();
	} else if(address >= USER_BASE && address <= USER_TOP) {
		aspace = vm_get_current_user_aspace();
		if(aspace == NULL) {
			if(is_user == false) {
				dprintf("vm_soft_fault: kernel thread accessing invalid user memory!\n");
				return ERR_VM_PF_FATAL;
			} else {
				// XXX weird state.
				panic("vm_soft_fault: non kernel thread accessing user memory that doesn't exist!\n");
			}
		}
	} else {
		// the hit was probably in the 64k DMZ between kernel and user space
		// this keeps a user space thread from passing a buffer that crosses into kernel space
		return ERR_VM_PF_FATAL;
	}
	map = &aspace->virtual_map;
	
	sem_acquire(map->sem, READ_COUNT);
	region = vm_virtual_map_lookup(map, address);
	if(region == NULL) {
		sem_release(map->sem, READ_COUNT);
		dprintf("vm_soft_fault: va 0x%x not covered by region in address space\n", address);
		return ERR_VM_PF_BAD_ADDRESS; // BAD_ADDRESS
	}
	
	// check permissions
	if(is_user && (region->lock & LOCK_KERNEL) == LOCK_KERNEL) {
		sem_release(map->sem, READ_COUNT);
		dprintf("user access on kernel region\n");
		return ERR_VM_PF_BAD_PERM; // BAD_PERMISSION
	}
	if(is_write && (region->lock & LOCK_RW) == 0) {
		sem_release(map->sem, READ_COUNT);
		dprintf("write access attempted on read-only region\n");
		return ERR_VM_PF_BAD_PERM; // BAD_PERMISSION
	}

	cache_ref = region->cache_ref;
	cache_offset = address - region->base + region->cache_offset;
	vm_cache_acquire_ref(cache_ref);
	change_count = map->change_count;
	sem_release(map->sem, READ_COUNT);

	// see if this cache has a fault handler
	if(cache_ref->cache->store->ops->fault) {
		int err = (*cache_ref->cache->store->ops->fault)(cache_ref->cache->store, aspace, cache_offset);
		vm_cache_release_ref(cache_ref);
		return err;
	}
	
	dummy_page.state = PAGE_STATE_INACTIVE;
	
	mutex_lock(&cache_ref->lock);

	for(;;) {
		page = vm_cache_lookup_page(cache_ref, cache_offset);
		if(page != NULL && page->state != PAGE_STATE_BUSY) {
			vm_page_set_state(page, PAGE_STATE_BUSY);
			mutex_unlock(&cache_ref->lock);
			break;
		}
		 
		if(page == NULL)
			break;
		 
		// page must be busy
		mutex_unlock(&cache_ref->lock);
		thread_snooze(20000);
		mutex_lock(&cache_ref->lock);
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
		mutex_unlock(&cache_ref->lock);
	}

	if(page == NULL) {
		// still haven't found a page, so zero out a new one
		page = vm_page_allocate_page(PAGE_STATE_CLEAR);
		dprintf("vm_soft_fault: just allocated page 0x%x\n", page->ppn);
		mutex_lock(&cache_ref->lock);
		vm_cache_remove_page(cache_ref, &dummy_page);
		vm_cache_insert_page(cache_ref, page, cache_offset);
		mutex_unlock(&cache_ref->lock);
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
			err = ERR_VM_PF_BAD_ADDRESS; // BAD_ADDRESS
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
		mutex_lock(&cache_ref->lock);
		vm_cache_remove_page(cache_ref, &dummy_page);
		mutex_unlock(&cache_ref->lock);
		dummy_page.state = PAGE_STATE_INACTIVE;
	}

	vm_page_set_state(page, PAGE_STATE_ACTIVE);
	
	vm_cache_release_ref(cache_ref);

	return err;
}

static vm_region *vm_virtual_map_lookup(vm_virtual_map *map, addr address)
{
	vm_region *region;

	// check the region_list region first
	region = map->region_hint;
	if(region && region->base <= address && (region->base + region->size) > address)
		return region;

	for(region = map->region_list; region != NULL; region = region->aspace_next) {
		if(region->base <= address && (region->base + region->size) > address)
			break;
	}

	if(region)
		map->region_hint = region;
	return region;
}

int vm_get_physical_page(addr paddr, addr *vaddr, int flags)
{
	return (*kernel_aspace->translation_map.ops->get_physical_page)(paddr, vaddr, flags);
}

int vm_put_physical_page(addr vaddr)
{
	return (*kernel_aspace->translation_map.ops->put_physical_page)(vaddr);
}

void vm_increase_max_commit(addr delta)
{
	int state;

//	dprintf("vm_increase_max_commit: delta 0x%x\n", delta); 

	state = int_disable_interrupts();
	acquire_spinlock(&max_commit_lock);
	max_commit += delta;
	release_spinlock(&max_commit_lock);
	int_restore_interrupts(state);
}

int user_memcpy(void *to, void *from, size_t size)
{
	return arch_cpu_user_memcpy(to, from, size, &thread_get_current_thread()->fault_handler);
}

int user_strcpy(char *to, const char *from)
{
	return arch_cpu_user_strcpy(to, from, &thread_get_current_thread()->fault_handler);
}

int user_strncpy(char *to, const char *from, size_t size)
{
	return arch_cpu_user_strncpy(to, from, size, &thread_get_current_thread()->fault_handler);
}

void *user_memset(void *s, char c, size_t count)
{
	return arch_cpu_user_memset(s, c, count, &thread_get_current_thread()->fault_handler);
}

