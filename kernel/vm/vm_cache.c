/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/vm_cache.h>
#include <kernel/vm_page.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/debug.h>
#include <kernel/lock.h>
#include <kernel/arch/cpu.h>
#include <sys/errors.h>

vm_cache *vm_cache_create(vm_store *store)
{
	vm_cache *cache;

	cache = kmalloc(sizeof(vm_cache));
	if(cache == NULL)
		return NULL;

	cache->page_list = NULL;
	cache->ref = NULL;
	cache->store = store;
	if(store != NULL)
		store->cache = cache;

	return cache;
}

vm_cache_ref *vm_cache_ref_create(vm_cache *cache)
{
	vm_cache_ref *ref;

	ref = kmalloc(sizeof(vm_cache_ref));
	if(ref == NULL)
		return NULL;

	ref->cache = cache;
	mutex_init(&ref->lock, "cache_ref_mutex");
	ref->region_list = NULL;
	ref->ref_count = 0;
	cache->ref = ref;

	return ref;
}

void vm_cache_acquire_ref(vm_cache_ref *cache_ref)
{
	if(cache_ref == NULL)
		panic("vm_cache_acquire_ref: passed NULL\n");
	atomic_add(&cache_ref->ref_count, 1);
}

void vm_cache_release_ref(vm_cache_ref *cache_ref)
{
	vm_page *page;

	if(cache_ref == NULL)
		panic("vm_cache_release_ref: passed NULL\n");
	if(atomic_add(&cache_ref->ref_count, -1) == 1) {
		// delete this cache
		// delete the cache's backing store, if it has one
		if(cache_ref->cache->store)
			(*cache_ref->cache->store->ops->destroy)(cache_ref->cache->store);

		// free all of the pages in the cache
		page = cache_ref->cache->page_list;
		while(page) {
			vm_page *old_page = page;
			page = page->cache_next;
			dprintf("vm_cache_release_ref: freeing page 0x%x\n", old_page->ppn);
			vm_page_set_state(old_page, PAGE_STATE_FREE);
		}

		mutex_destroy(&cache_ref->lock);
		kfree(cache_ref->cache);
		kfree(cache_ref);
	}
}

vm_page *vm_cache_lookup_page(vm_cache_ref *cache_ref, off_t offset)
{
	vm_page *page;

	for(page = cache_ref->cache->page_list; page != NULL; page = page->cache_next) {
		if(page->offset == offset) {
			return page;
		}
	}

	return NULL;
}

void vm_cache_insert_page(vm_cache_ref *cache_ref, vm_page *page, off_t offset)
{
	page->offset = offset;

	if(cache_ref->cache->page_list != NULL) {
		cache_ref->cache->page_list->cache_prev = page;
	}
	page->cache_next = cache_ref->cache->page_list;
	page->cache_prev = NULL;
	cache_ref->cache->page_list = page;

	page->cache_ref = cache_ref;
}

void vm_cache_remove_page(vm_cache_ref *cache_ref, vm_page *page)
{
	if(cache_ref->cache->page_list == page) {
		if(page->cache_next != NULL)
			page->cache_next->cache_prev = NULL;
		cache_ref->cache->page_list = page->cache_next;
	} else {
		if(page->cache_prev != NULL)
			page->cache_prev->cache_next = page->cache_next;
		if(page->cache_next != NULL)
			page->cache_next->cache_prev = page->cache_prev;
	}
	page->cache_ref = NULL;
}

int vm_cache_insert_region(vm_cache_ref *cache_ref, vm_region *region)
{
	mutex_lock(&cache_ref->lock);

	region->cache_next = cache_ref->region_list;
	if(region->cache_next)
		region->cache_next->cache_prev = region;
	region->cache_prev = NULL;
	cache_ref->region_list = region;

	mutex_unlock(&cache_ref->lock);
	return 0;
}

int vm_cache_remove_region(vm_cache_ref *cache_ref, vm_region *region)
{
	mutex_lock(&cache_ref->lock);

	if(region->cache_prev)
		region->cache_prev->cache_next = region->cache_next;
	if(region->cache_next)
		region->cache_next->cache_prev = region->cache_prev;
	if(cache_ref->region_list == region)
		cache_ref->region_list = region->cache_next;

	mutex_unlock(&cache_ref->lock);
	return 0;
}
