/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/vm_cache.h>
#include <kernel/vm_page.h>
#include <kernel/heap.h>
#include <kernel/sem.h>
#include <kernel/debug.h>
#include <kernel/arch/cpu.h>

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
	ref->sem = sem_create(1, "cache_ref_sem");
//	if(ref->sem < 0) {
//		kfree(ref);
//		return NULL;
//	}
	ref->region_list = NULL;
	ref->ref_count = 0;

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

		sem_delete(cache_ref->sem);
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
			page->cache_next->cache_prev = page->cache_next;		
	}
	page->cache_ref = NULL;
}
