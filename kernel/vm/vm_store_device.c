/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/vm_store_device.h>

struct device_store_data {
	addr base_addr;
};

static void device_destroy(struct vm_store *store)
{
	if(store) {
		kfree(store);
	}
}

static off_t device_commit(struct vm_store *store, off_t size)
{
	return 0;
}

static int device_has_page(struct vm_store *store, off_t offset)
{
	// this should never be called
	return 0;
}

static int device_read(struct vm_store *store, off_t offset, void *buf, size_t *len)
{
	panic("device_store: read called. Invalid!\n");
}

static int device_write(struct vm_store *store, off_t offset, const void *buf, size_t *len)
{
	panic("device_store: write called. Invalid!\n");
}

// this fault handler should take over the page fault routine and map the page in
//
// setup: the cache that this store is part of has a ref being held and will be
// released after this handler is done
static int device_fault(struct vm_store *store, struct vm_address_space *aspace, off_t offset)
{
	struct device_store_data *d = (struct device_store_data *)store->data;
	addr vaddr, paddr;
	
	dprintf("device_fault: offset 0x%d + base_addr 0x%x\n", offset, d->base_addr);
	
	// figure out which page needs to be mapped where
	
// XXX finish

#if 0	
	// simply map in the appropriate page, regardless of vm_page coverage
	(*aspace->translation_map.ops->lock)(&aspace->translation_map);
	(*aspace->translation_map.ops->map)(&aspace->translation_map, address,
		page->ppn * PAGE_SIZE, region->lock);
	(*aspace->translation_map.ops->unlock)(&aspace->translation_map);
#endif
	
	panic("here\n");
	return 0;
}

static vm_store_ops device_ops = {
	&device_destroy,
	&device_commit,
	&device_has_page,
	&device_read,
	&device_write,
	&device_fault
};

vm_store *vm_store_create_device(addr base_addr)
{
	vm_store *store;
	struct device_store_data *d;
	
	store = kmalloc(sizeof(vm_store) + sizeof(struct device_store_data));
	if(store == NULL)
		return NULL;

	store->ops = &device_ops;
	store->cache = NULL;
	store->data = (void *)((addr)store + sizeof(vm_store));

	d = (struct device_store_data *)store->data;
	d->base_addr = base_addr;

	return store;
}

